module;

#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thirdparty/libgit2/include/git2.h>
#include <unordered_map>

export module gitkf:git_repository;
import :client_exception;
import :git_smart_pointer;
import :ring_buffer;
import :string_utils;

export struct GitRef {
    std::string name {};
    std::string id {};
    bool isTag {};
    bool isBranch {};
    bool isRemote {};
};

export template <size_t N>
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

export class GitRepository {
public:
    explicit GitRepository(const std::string& repoPath)
    {
        git_buf buf {};
        std::unique_ptr<git_buf> pBuf { &buf };
        if (git_repository_discover(pBuf.get(), repoPath.c_str(), /*across_fs=*/false, /*ceiling_dirs=*/nullptr)) {
            throw ClientException(404, std::format("Git repo is not found under '{}'.", repoPath));
        }

        m_repoPath = pBuf->ptr;
        if (git_repository_open(std::out_ptr(m_pRepo), pBuf->ptr)) {
            throw std::runtime_error { std::format("Open git repo '{}' failed.", m_repoPath) };
        }
        while (m_repoPath.ends_with('/')) {
            m_repoPath.pop_back();
        }

        // Remove ".git" part.
        m_workDir = std::filesystem::path { m_repoPath }.parent_path().string();
    }

    git_repository* Get() const { return m_pRepo.get(); }
    git_repository* GetRepo() const { return m_pRepo.get(); }
    const std::string& GetRepoRoot() const { return m_repoPath; }
    const std::string& GetRepoWorkDir() const { return m_workDir; }

    std::unordered_multimap<std::string, GitRef> GetRefs()
    {
        std::unordered_multimap<std::string, GitRef> refs {};
        auto payload = std::make_pair(&refs, m_pRepo.get());
        git_reference_foreach(
            m_pRepo.get(),
            [](git_reference* pReference, void* payload) {
                auto pPair = (std::pair<std::unordered_multimap<std::string, GitRef>*, git_repository*>*)payload;
                auto* pRefs = pPair->first;
                auto* pRepo = pPair->second;
                auto name = git_reference_name(pReference);
                git_oid oid {};
                git_reference_name_to_id(&oid, pRepo, name);
                auto id = GitHashToString(oid.id);
                GitRef ref {};
                ref.name = TrimLeft(name, { "refs/heads/", "refs/remotes/", "refs/tags/" });
                ref.id = id;
                ref.isTag = git_reference_is_tag(pReference);
                ref.isBranch = git_reference_is_branch(pReference);
                ref.isRemote = git_reference_is_remote(pReference);
                pRefs->emplace(std::move(id), std::move(ref));
                git_reference_free(pReference);
                return 0;
            },
            &payload);
        return refs;
    }

private:
    std::string m_repoPath {};
    std::string m_workDir {};
    std::unique_ptr<git_repository> m_pRepo {};
};

export std::shared_ptr<GitRepository> GetSharedGitRepository(const std::string& repoPath)
{
    static std::shared_mutex s_repository_cache_mutex {};
    static ring_buffer<std::pair<std::string, std::shared_ptr<GitRepository>>, 50> s_repo_cache;

    // If the repository existed in the cache, return it.
    {
        std::shared_lock read_lock { s_repository_cache_mutex };
        auto it = std::find_if(
            s_repo_cache.begin(), s_repo_cache.end(), [&repoPath](const auto& pair) { return pair.first == repoPath; });
        if (it != s_repo_cache.end()) {
            return it->second;
        }
    }

    // Create a new one add to the cache.
    std::unique_lock write_lock { s_repository_cache_mutex };

    // Maybe added by other thread already, let's double check.
    auto it = std::find_if(
        s_repo_cache.begin(), s_repo_cache.end(), [&repoPath](const auto& pair) { return pair.first == repoPath; });
    if (it != s_repo_cache.end()) {
        return it->second;
    }

    // Really create a new one.
    auto pRepo = std::make_shared<GitRepository>(repoPath);
    s_repo_cache.push_front(std::make_pair(repoPath, pRepo));
    return pRepo;
}