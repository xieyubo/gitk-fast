module;

#include "src/res/res.h"
#include "thirdparty/httplib.h"
#include "thirdparty/json.hpp"
#include "thirdparty/libgit2/include/git2.h"
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <format>
#include <unordered_map>

#define const const char*
#include "src/res/wwwroot/version.js"
#undef const

export module gitkf:gitkf;
import :client_exception;
import :git_smart_pointer;
import :git_repository;
import :line_reader;
import :option;
import :platform_utils;

using json = nlohmann::json;

const int kNoParent = -1;
const int kNotInTheRange = -2;
const std::string_view kPatchFileDiffHeaderLine = "diff --git a/";

struct GitCommit {
    std::string id {};
    git_commit* commit {};
    int column { -1 };
    int parentIndexes[2] = { kNoParent, kNoParent };
    int minReservedColumn {};
    int maxReservedColumn { -1 };
};

template <size_t N>
std::string GitHashToString(const unsigned char (&hash)[N])
{
    static const char kHex[] = "0123456789abcdef";
    std::string res(40, '\0');
    auto* pData = res.data();
    for (auto ch : hash) {
        *pData++ = kHex[ch >> 4];
        *pData++ = kHex[ch & 0xf];
    }
    return res;
}

git_oid StringToGitHash(const std::string& hash)
{
    git_oid oid {};
    for (auto i = 0u; i < 20; ++i) {
        auto j = i * 2;
        oid.id[i] = (unsigned char)(((hash[j] >= 'a' ? (hash[j] - 'a' + 10) : hash[j] - '0') << 4)
            | (hash[j + 1] >= 'a' ? (hash[j + 1] - 'a' + 10) : hash[j + 1] - '0'));
    }
    return oid;
}

json serialize(const git_signature& signature)
{
    json r;
    r["name"] = signature.name;
    r["email"] = signature.email;
    return r;
}

json serialize(const git_commit* pCommit)
{
    auto message = std::string_view { git_commit_message(pCommit) };
    auto firstLineMessage = message.substr(0, message.find('\n'));
    auto pSignature = git_commit_author(pCommit);
    auto time = git_commit_time(pCommit);
    const auto& pTime = std::localtime(&time);

    json r;
    r["summary"] = firstLineMessage;
    r["author"] = serialize(*pSignature);
    r["date"] = std::format("{}-{:02}-{:02} {:02}:{:02}:{:02}", pTime->tm_year + 1900, pTime->tm_mon + 1,
        pTime->tm_mday, pTime->tm_hour, pTime->tm_min, pTime->tm_sec);
    return r;
}

json serialize(const GitCommit& commit, bool graphInfoOnly)
{
    auto r = graphInfoOnly ? json {} : serialize(commit.commit);
    r["id"] = commit.id;
    r["column"] = commit.column;
    r["parentIndexes"] = { commit.parentIndexes[0], commit.parentIndexes[1] };
    r["minReservedColumn"] = commit.minReservedColumn;
    r["maxReservedColumn"] = commit.maxReservedColumn;
    return r;
}

static std::string dump(const json& json)
{
    return json.dump(
        /*indent=*/-1, /*indent_char=*/' ', /*ensure_ascii=*/false, /*error_handler=*/json::error_handler_t::replace);
}

std::string serialize(
    std::vector<GitCommit>::const_iterator begin, std::vector<GitCommit>::const_iterator end, bool graphInfoOnly)
{
    json j;
    for (auto it = begin; it != end; ++it) {
        j.push_back(serialize(*it, graphInfoOnly));
    }
    return dump(j);
}

json parse_patch(std::string filename, std::vector<std::string> diffs)
{
    auto header = std::format("-------------------------------- {} --------------------------------\n", filename);

    auto i = 0u;

    // Parse header.
    for (; i < diffs.size(); ++i) {
        if (diffs[i].starts_with("@@ ")) {
            break;
        }

        header += diffs[i];
        header += '\n';
    }

    json patch {};
    patch["filename"] = std::move(filename);

    json chunks {};
    json headerChunk {};
    headerChunk["type"] = "header";
    headerChunk["content"] = std::move(header);
    chunks.push_back(std::move(headerChunk));

    enum class ChunkType {
        Default,
        Statistics,
        Add,
        Delete,
    };

    auto chunkTypeToString = [](ChunkType type) {
        switch (type) {
        default:
        case ChunkType::Default:
            return "default";

        case ChunkType::Statistics:
            return "statistics";

        case ChunkType::Add:
            return "add";

        case ChunkType::Delete:
            return "delete";
        }
    };

    auto createChunk = [&chunkTypeToString](ChunkType type, std::string content) {
        json chunk {};
        chunk["type"] = chunkTypeToString(type);
        chunk["content"] = std::move(content);
        return chunk;
    };

    ChunkType currentChunkType {};
    std::string currentContent {};
    for (; i < diffs.size(); ++i) {
        ChunkType type {};
        if (diffs[i].starts_with("@@ ")) {
            type = ChunkType::Statistics;
        } else {
            auto ch = *diffs[i].c_str();
            type = ch == '+' ? ChunkType::Add : (ch == '-' ? ChunkType::Delete : ChunkType::Default);
        }
        if (type != currentChunkType) {
            if (!currentContent.empty()) {
                chunks.push_back(createChunk(currentChunkType, std::move(currentContent)));
                assert(currentContent.empty());
            }
            currentChunkType = type;
        }
        currentContent += diffs[i];
        currentContent += '\n';
    }

    if (!currentContent.empty()) {
        chunks.push_back(createChunk(currentChunkType, std::move(currentContent)));
    }
    patch["chunks"] = std::move(chunks);
    return patch;
}

json parse_patch(const std::string& str)
{
    json patch {};
    std::istringstream in { str };
    std::string line;
    while (!line.empty() || std::getline(in, line)) {
        while (!line.starts_with(kPatchFileDiffHeaderLine)) {
            if (!std::getline(in, line)) {
                return patch;
            }
        }

        // Find a file patch, get file name first.
        auto spacePos = line.find(' ', kPatchFileDiffHeaderLine.size());
        if (spacePos == std::string::npos) {
            // Invalid diff format.
            continue;
        }

        auto filename = line.substr(kPatchFileDiffHeaderLine.size(), spacePos - kPatchFileDiffHeaderLine.size());
        line.clear();

        std::vector<std::string> diffs;
        while (std::getline(in, line) && !line.starts_with(kPatchFileDiffHeaderLine)) {
            diffs.emplace_back(std::move(line));
        }
        patch.push_back(parse_patch(std::move(filename), std::move(diffs)));
    }
    return patch;
}

std::string create_author_or_committer_line(const git_signature* pSignature)
{
    const auto& pTime = std::localtime(&pSignature->when.time);
    return std::format("{} <{}> {}-{:02}-{:02} {:02}:{:02}:{:02}", pSignature->name, pSignature->email,
        pTime->tm_year + 1900, pTime->tm_mon + 1, pTime->tm_mday, pTime->tm_hour, pTime->tm_min, pTime->tm_sec);
}

json create_parent_node(const git_commit* pCommit, int parentIndex)
{
    std::unique_ptr<git_commit> pParent;
    git_commit_parent(std::out_ptr(pParent), pCommit, parentIndex);

    json j {};
    if (pParent) {
        j["id"] = GitHashToString(git_commit_id(pParent.get())->id);

        auto message = std::string_view { git_commit_message(pParent.get()) };
        auto firstLineMessage = message.substr(0, message.find('\n'));
        j["summary"] = std::move(firstLineMessage);
    }
    return j;
}

std::string create_message_lines(const git_commit* pCommit)
{
    auto message = std::string { git_commit_message(pCommit) };
    std::istringstream in { std::move(message) };
    std::string line;
    while (std::getline(in, line)) {
        message += '\t';
        message += line;
        message += '\n';
    }
    return message;
}

void create_detail_header(json& j, git_commit* pCommit)
{
    if (auto pAuthor = git_commit_author(pCommit)) {
        j["author"] = create_author_or_committer_line(pAuthor);
    }

    if (auto pCommitter = git_commit_committer(pCommit)) {
        j["committer"] = create_author_or_committer_line(pCommitter);
    }

    json parents;
    if (auto node = create_parent_node(pCommit, 0); !node.empty()) {
        parents.push_back(std::move(node));
    }
    if (auto node = create_parent_node(pCommit, 1); !node.empty()) {
        parents.push_back(std::move(node));
    }
    if (!parents.empty()) {
        j["parents"] = std::move(parents);
    }

    j["message"] = create_message_lines(pCommit);
}

std::string get_git_commit(
    const std::string& repoPath, const std::string& follow, const std::string& commitId, bool ignoreWhitespace)
{
    auto pGit = GetSharedGitRepository(repoPath);

    auto oid = StringToGitHash(commitId);
    std::unique_ptr<git_commit> pCommit {};
    if (git_commit_lookup(std::out_ptr(pCommit), pGit->GetRepo(), &oid)) {
        return "{}";
    }
    assert(pCommit);

    json j;
    j["id"] = commitId;

    // Get metadata
    create_detail_header(j, pCommit.get());

    // Get diff
    auto cmd = std::format("git show --pretty=format: {}", commitId);
    if (ignoreWhitespace) {
        cmd += " -w";
    }
    if (!follow.empty()) {
        cmd += " -- " + follow;
    }
    j["patch"] = parse_patch(ExternRun(cmd, repoPath.c_str()));

    return dump(j);
}

void get_git_log(httplib::DataSink& sink, git_repository* repo, const std::string& repoPath, const std::string& follow,
    bool noMerges, const std::string& commitId, const std::vector<std::string>& authors)
{
    git_revwalk* walk {};
    auto r = git_revwalk_new(&walk, repo);
    // git_revwalk_sorting(walk, GIT_SORT_TIME);
    git_revwalk_push_head(walk);
    std::vector<GitCommit> commits;
    std::unordered_map<std::string, int> hashToCommitIndex;

    auto count = 500;
    commits.reserve(count);
    auto cmd = std::format("git log -n {} --pretty=format:%H", count);
    if (noMerges) {
        cmd += " --no-merges";
    }
    if (!commitId.empty()) {
        cmd += " " + commitId;
    }
    for (const auto& author : authors) {
        cmd += std::format(" --author \"{}\"", author);
    }
    if (!follow.empty()) {
        cmd += " -- " + follow;
    }

    std::vector<int> avaliable_columns;
    int next_avaliable_columnt {};
    int max_possible_columnt {};
    auto lineReader = LineReader {};
    auto get_avaliable_column = [&] {
        if (avaliable_columns.empty()) {
            auto column = next_avaliable_columnt++;
            max_possible_columnt = std::max(max_possible_columnt, column);
            return column;
        } else {
            auto i = *avaliable_columns.rbegin();
            avaliable_columns.pop_back();
            return i;
        }
    };

    auto free_column = [&](int column) {
        if (column == next_avaliable_columnt - 1) {
            --next_avaliable_columnt;
        } else {
            avaliable_columns.push_back(column);
        }
    };

    auto processLines = [&]() {
        size_t startPos = commits.size();
        while (auto pLine = lineReader.GetLine()) {
            if (pLine->empty()) {
                continue;
            }

            git_commit* commit {};
            auto oid = StringToGitHash(*pLine);
            git_commit_lookup(&commit, repo, &oid);
            GitCommit v {};
            auto* poid = git_commit_id(commit);
            v.id = GitHashToString(poid->id);
            v.commit = commit;
            commits.emplace_back(std::move(v));
            hashToCommitIndex.emplace(commits.rbegin()->id, (int)commits.size() - 1);
        }

        avaliable_columns.clear();
        next_avaliable_columnt = 0;
        for (auto& commit : commits) {
            commit.column = -1;
            commit.parentIndexes[0] = kNoParent;
            commit.parentIndexes[1] = kNoParent;
            commit.minReservedColumn = 0;
            commit.maxReservedColumn = -1;
        }
        for (auto& commit : commits) {
            if (commit.column < 0) {
                commit.column = get_avaliable_column();
            }
            auto parentCount = git_commit_parentcount(commit.commit);
            for (auto i = 0u; i < parentCount; ++i) {
                auto parentOid = git_commit_parent_id(commit.commit, i);
                auto it = hashToCommitIndex.find(GitHashToString(parentOid->id));
                if (it != hashToCommitIndex.end()) {
                    auto& parent = commits[it->second];
                    if (parent.column == -1) {
                        if (commit.parentIndexes[0] < 0 || commits[commit.parentIndexes[0]].column != commit.column) {
                            parent.column = commit.column;
                        } else {
                            parent.column = get_avaliable_column();
                        }
                    }
                    commit.parentIndexes[i] = it->second;
                } else {
                    commit.parentIndexes[i] = kNotInTheRange;
                }
            }
            commit.maxReservedColumn = next_avaliable_columnt - 1;

            if ((commit.parentIndexes[0] < 0 || commits[commit.parentIndexes[0]].column != commit.column)
                && (commit.parentIndexes[1] < 0 || commits[commit.parentIndexes[1]].column != commit.column)) {
                free_column(commit.column);
            }
        }

        std::vector<int> maxReservedColumnTracker(max_possible_columnt + 1);
        for (auto i = 0u; i < commits.size(); ++i) {
            maxReservedColumnTracker[commits[i].column] = i;
        }

        for (auto i = 0u; i < commits.size(); ++i) {
            auto& commit = commits[i];
            while (commit.minReservedColumn < commit.column) {
                if (maxReservedColumnTracker[commit.minReservedColumn] < i) {
                    ++commit.minReservedColumn;
                } else {
                    break;
                }
            }
            while (commit.maxReservedColumn > commit.column) {
                if (maxReservedColumnTracker[commit.maxReservedColumn] < i) {
                    --commit.maxReservedColumn;
                } else {
                    break;
                }
            }
        }

        if (startPos < commits.size()) {
            auto commitsData = serialize(commits.begin() + startPos, commits.end(), /*graphInfoOnly=*/false);
            auto graphData = serialize(commits.begin(), commits.end(), /*graphInfoOnly=*/true);
            auto event = std::format("data: {{\"commits\": {}, \"graphs\": {}}}\n\n", commitsData, graphData);
            return sink.write(event.c_str(), event.size());
        } else {
            return true;
        }
    };

    ExternRun(cmd, repoPath.c_str(), [&](char* data, size_t size) {
        lineReader.Append(data, size);
        return processLines();
    });

    // Process the last line which may not contains \n;
    lineReader.Append("\n", 2);
    processLines();

    // Send end data.
    auto event = std::string { "data: {\"commits\": [], \"graphs\": []}\n\n" };
    sink.write(event.c_str(), event.size());
}

void get_git_log(httplib::DataSink& sink, const std::string& repoPath, const std::string& path, bool noMerges,
    const std::string& commitId, const std::vector<std::string>& authors)
{
    auto pGit = GetSharedGitRepository(repoPath);
    get_git_log(sink, pGit->GetRepo(), pGit->GetRepoRoot(),
        std::filesystem::relative(path, pGit->GetRepoWorkDir()).string(), noMerges, commitId, authors);
}

static const std::string GetHttpQueryParameter(
    const httplib::Request& req, const std::string& key, std::string&& defaultValue)
{
    auto it = req.params.find(key);
    return it == req.params.end() ? std::move(defaultValue) : it->second;
}

static std::vector<std::string> GetHttpQueryParameters(const httplib::Request& req, const std::string& key)
{
    std::vector<std::string> res {};
    auto values = req.params.equal_range(key);
    for (auto it = values.first; it != values.second; ++it) {
        res.emplace_back(it->second);
    }
    return res;
}

/// @brief Process static file request handler. If the file is not embedded, return Unhandled so that the request
///        will be processed by the next handler.
static httplib::Server::HandlerResponse ProcessStaticFileRequest(const httplib::Request& req, httplib::Response& res)
{
    std::string path = req.path == "/" ? "wwwroot/index.html" : "wwwroot" + req.path;
    auto fs = cmrc::res::get_filesystem();
    if (fs.exists(path)) {
        auto file = fs.open(path);
        res.set_content(
            file.begin(), file.size(), httplib::detail::find_content_type(path, {}, "application/octet-stream"));
        return httplib::Server::HandlerResponse::Handled;
    }
    return httplib::Server::HandlerResponse::Unhandled;
}

/// @brief Handle get git log request. Request path is: /api/git-log?repo=...&path=...
static void ProcessGetGitLogRequest(const httplib::Request& req, httplib::Response& res)
{
    auto repo = GetHttpQueryParameter(req, "repo", "");
    if (repo.empty()) {
        res.status = httplib::StatusCode::NotFound_404;
        return;
    }
    auto path = GetHttpQueryParameter(req, "path", "");
    auto noMerges = GetHttpQueryParameter(req, "noMerges", "") == "1";
    auto commitId = GetHttpQueryParameter(req, "commit", "");
    auto authors = GetHttpQueryParameters(req, "author");
    res.set_content_provider("text/event-stream",
        [repo = std::move(repo), path = std::move(path), noMerges = std::move(noMerges), commitId = std::move(commitId),
            authors = std::move(authors)](size_t offset, httplib::DataSink& sink) {
            get_git_log(sink, repo, path, noMerges, commitId, authors);
            return false;
        });
}

/// @brief Handle get git commit detail request. Request path is: /api/git-commit/{commitId}
static void ProcessGetGitCommitRequest(const httplib::Request& req, httplib::Response& res)
{
    auto repo = GetHttpQueryParameter(req, "repo", "");
    if (repo.empty()) {
        res.status = httplib::StatusCode::NotFound_404;
        return;
    }

    auto commitId = req.path_params.at("commitId");
    auto path = GetHttpQueryParameter(req, "path", "");
    auto ignoreWhitespace = GetHttpQueryParameter(req, "ignoreWhitespace", "") == "1";
    res.set_content(get_git_commit(repo, path, commitId, ignoreWhitespace), "application/json");
}

/// @brief Start server. This function won't return until the server is stopped (currently, we never stop server).
static int StartServer(const Option& option)
{
    // Init libgit2.
    git_libgit2_init();

    // Create http server.
    httplib::Server svr {};
    svr.set_keep_alive_max_count(std::numeric_limits<int32_t>::max());
    svr.set_keep_alive_timeout(std::numeric_limits<int32_t>::max());

    // If wwwroot is specified, mount it, otherwise, use the embedded files.
    if (option.wwwroot.empty()) {
        svr.set_pre_routing_handler(ProcessStaticFileRequest);
    } else {
        svr.set_mount_point("/", option.wwwroot);
    }

    // Add get git log request handler.
    svr.Get("/api/git-log", ProcessGetGitLogRequest);

    // Add get git commit detail handler.
    svr.Get("/api/git-commit/:commitId", ProcessGetGitCommitRequest);

    // Start server. Note, this function wont return until the server is stopped (currently, we never stop server).
    printf("gitkf server is running...\n");
    svr.listen("localhost", option.port);

    // Shutdown libgit2.
    git_libgit2_shutdown();
    return 0;
}

export int gitkf_main(int argc, char* argv[])
{
    // Parse option.
    auto option = Option::Parse(argc - 1, argv + 1);
    if (option.printVersion) {
        printf("gitkf (https://github.com/xieyubo/gitk-fast): ver %s\n", kVersion);
        return 0;
    }

    if (option.serverMode) {
        return StartServer(option);
    } else {
        // Check the application is running or not.
        auto client = httplib::Client { std::format("http://localhost:{}", option.port) };
        client.set_connection_timeout(0, 1'000'000);
        auto res = client.Get("/");
        if (!res) {
            // No server is running, start it.
            RunAsDaemon(std::format("{} --server", get_current_app_full_path()));
        }

        // Server is running started, open the url.
        auto url = std::format("http://localhost:{}?repo={}", option.port, option.repoPath);
        if (!option.follow.empty()) {
            url += std::format("&path={}", option.follow);
        }
        if (!option.commitId.empty()) {
            url += std::format("&commit={}", option.commitId);
        }
        for (const auto& author : option.authors) {
            url += std::format("&author={}", author);
        }
        OpenUrl(url);
        return 0;
    }
}