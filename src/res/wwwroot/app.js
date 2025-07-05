import "./components.js";

const server = location.origin;
const queries = new URLSearchParams(location.search);
const g_repo = queries.get("repo") || "";
const g_path = queries.get("path") || "";
const g_commitId = queries.get("commit") || "";
const g_author = queries.get("author") || "";
var g_app;
var g_ignoreWhitespaceCheckbox;
var g_noMergesCheckbox;

window.onload = async () => {
    window.app = g_app = new App();
    g_noMergesCheckbox = document.getElementById("no-merges-checkbox");
    g_ignoreWhitespaceCheckbox = document.getElementById("ignore-whitespace-checkbox");

    var verDom = document.getElementById("current-version-column");
    verDom.innerText = `Current Ver: ${kVersion}`;

    fetch("https://raw.githubusercontent.com/xieyubo/gitk-fast/refs/heads/release/VERSION")
        .then(r => {
            if (r.ok) {
                r.text().then(newVersion => {
                    if (newVersion != kVersion) {
                        var newVerDom = document.createElement("div");
                        newVerDom.innerHTML = `<a class="clickable-text" href="https://github.com/xieyubo/gitk-fast/releases" target=_blank>New Ver: ${newVersion}</a>`;
                        document.getElementById("footer").appendChild(newVerDom);
                    }
                })
            }
        });

    await g_app.load_commits_async();
}

const kColumnColors
    = [
        "#06a77d",
        "#ff6347",
        "#0066cc",
        "#6666ff",
        "#9900cc",
        "#993366",
        "#996633",
        "#669900",
        "#b37700",
        "#ff6666",
        "#ff9900",
        "#00e6e6",
        "#999966",
        "#008000",
        "#669999",
        "#333399",
    ];

const columnEndedTrack = {};

const kLineHight = 28;

function show_detail_loading_wrapper(show) {
    if (show) {
        document.getElementById("commit-detail-waitting-cover").classList.remove("hidden");
        document.getElementById("commit-file-list-waitting-cover").classList.remove("hidden");
    } else {
        document.getElementById("commit-detail-waitting-cover").classList.add("hidden");
        document.getElementById("commit-file-list-waitting-cover").classList.add("hidden");
    }
}

function show_commits_loading_wrapper(show) {
    if (show) {
        document.getElementById("gitk-history-content-waitting-cover").classList.remove("hidden");
    } else {
        document.getElementById("gitk-history-content-waitting-cover").classList.add("hidden");
    }
}

function is_column_ended(commit, commits) {
    return (commit.parentIndexes[0] < 0 || commits[commit.parentIndexes[0]].column != commit.column)
        && (commit.parentIndexes[1] < 0 || commits[commit.parentIndexes[1]].column != commit.column);
}

function crate_graph(commit, commits) {
    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.classList.add("graph");
    svg.setAttribute("width", `${(commit.maxReservedColumn + 1) * 14 + 7}`);
    svg.setAttribute("height", `${kLineHight}`);

    // Draw normal vertical line
    for (var i = commit.minReservedColumn; i <= commit.maxReservedColumn; ++i) {
        const color = kColumnColors[i % kColumnColors.length];
        if (columnEndedTrack[i] != undefined && !columnEndedTrack[i]) {
            const polyline = document.createElementNS("http://www.w3.org/2000/svg", "polyline");
            const x = (i + 1) * 14;
            const y = columnEndedTrack[i] == undefined || columnEndedTrack[i] ? kLineHight / 2 : 0;
            const yEnd = i == commit.column && is_column_ended(commit, commits) ? kLineHight / 2 : kLineHight;
            polyline.setAttribute("points", `${x},${y} ${x},${yEnd}`);
            polyline.setAttribute("style", `fill: none; stroke: ${color}; stroke-width: 2;`);
            svg.appendChild(polyline);
        }
    }

    // Draw parent line
    for (var i = 0; i < 2; ++i) {
        const parentIndex = commit.parentIndexes[i];
        if (parentIndex != -1) {
            const parentColumn = parentIndex >= 0 ? commits[parentIndex].column : commit.column;
            const color = kColumnColors[parentColumn % kColumnColors.length];
            const polyline = document.createElementNS("http://www.w3.org/2000/svg", "polyline");
            const x = (commit.column + 1) * 14;
            const parentX = (parentColumn + 1) * 14;
            const parentY = parentIndex >= 0 ? kLineHight : kLineHight - 6;
            polyline.setAttribute("points", `${x},${kLineHight / 2} ${parentX},${parentY}`);
            polyline.setAttribute("style", `fill: none; stroke: ${color}; stroke-width: 2;`);
            svg.appendChild(polyline);
            if (parentIndex >= 0) {
                columnEndedTrack[parentColumn] = false;
            } else {
                const arrow = document.createElementNS("http://www.w3.org/2000/svg", "polygon");
                arrow.setAttribute(
                    "points", `${parentX - 4},${parentY} ${parentX + 4},${parentY} ${parentX},${parentY + 6}`);
                arrow.setAttribute("style", `fill: ${color};`);
                svg.appendChild(arrow);
            }
        }
    }

    // Draw circle at this column.
    const color = kColumnColors[commit.column % kColumnColors.length];
    const circle = document.createElementNS("http://www.w3.org/2000/svg", "circle");
    circle.setAttribute("cx", `${(commit.column + 1) * 14}`);
    circle.setAttribute("cy", `${kLineHight / 2}`);
    circle.setAttribute("r", "3");
    circle.setAttribute("fill", color);
    circle.setAttribute("stroke", color);
    circle.setAttribute("stroke-width", "2");
    svg.appendChild(circle);
    columnEndedTrack[commit.column] = is_column_ended(commit, commits);
    return svg;
}

function crate_message(commit) {
    const message = document.createElement("div");
    message.classList.add("message");
    var txt = document.createTextNode(commit.summary);
    message.appendChild(txt);
    return message;
}

function create_commit_navigation_node(label, commit) {
    const dom = document.createElement("div");
    dom.classList.add("nowrap");
    dom.appendChild(document.createTextNode(`${label}: `));

    const span = document.createElement("span");
    span.innerText = commit.id;
    if (document.getElementById(`commit-${commit.id}`)) {
        span.classList.add("clickable-text");
        span.addEventListener("click", () => g_app.select_commit(commit.id));
    }
    dom.appendChild(span);

    dom.appendChild(document.createTextNode(` (${commit.summary})`));
    return dom;
}

function create_commit_detail(commit, detailPanelDom, fileListDom) {
    var detailDomList = [];
    var fileDomList = [];

    // Add comments node.
    const commentsDetailDom = document.createElement("div");
    commentsDetailDom.classList.add("comments");
    if (commit.author) {
        const dom = document.createElement("div");
        dom.classList.add("nowrap");
        dom.innerText = `Author: ${commit.author}`;
        commentsDetailDom.appendChild(dom);
    }
    if (commit.committer) {
        const dom = document.createElement("div");
        dom.classList.add("nowrap");
        dom.innerText = `Committer: ${commit.committer}`;
        commentsDetailDom.appendChild(dom);
    }
    if (commit.parents) {
        commit.parents.forEach(
            parent => commentsDetailDom.appendChild(create_commit_navigation_node("Parent", parent)));
    }
    g_app.find_children(commit.id).forEach(
        child => commentsDetailDom.appendChild(create_commit_navigation_node("Child", child)));

    const message = document.createElement("pre");
    message.innerText = `Follows:\nPreceedes:\n\n${commit.message}`;
    commentsDetailDom.appendChild(message);
    detailDomList.push(commentsDetailDom);

    const dom = document.createElement("div");
    dom.classList.add("file");
    dom.addEventListener("click", () => onSelectFile(dom));
    dom.innerText = "Comments";
    dom.detailDom = commentsDetailDom;
    fileDomList.push(dom);

    // Add file node and file detail.
    if (commit.patch) {
        commit.patch.forEach(patch => {
            // Add detail dom.
            const fileDiffDom = document.createElement("div");
            fileDiffDom.classList.add("file-diff");
            patch.chunks.forEach(chunk => {
                const dom = document.createElement("pre");
                dom.classList.add(`chunk-${chunk.type}`);
                dom.innerText = chunk.content;
                fileDiffDom.appendChild(dom);
            });
            detailDomList.push(fileDiffDom);

            // Add file dome
            const file = patch.filename;
            const fileDom = document.createElement("div");
            fileDom.classList.add("file");
            fileDom.addEventListener("click", () => onSelectFile(fileDom));
            fileDom.innerText = file;
            fileDom.detailDom = fileDiffDom;
            fileDomList.push(fileDom);
        });
    }
    detailPanelDom.replaceChildren(...detailDomList);
    fileListDom.replaceChildren(...fileDomList);
}

function onSelectFile(fileDom) {
    if (onSelectFile.lastSelectedFileDom) {
        onSelectFile.lastSelectedFileDom.classList.remove("selected");
    }
    fileDom.classList.add("selected");
    onSelectFile.lastSelectedFileDom = fileDom;
    fileDom.detailDom.scrollIntoView();
}

function clean_commit_detail() {
    const detailPanelDom = document.getElementById("commit-detail");
    const fileListDom = document.getElementById("commit-file-list");
    const commitIdFieldDom = document.getElementById("commit-id-field");
    detailPanelDom.replaceChildren();
    fileListDom.replaceChildren();
    commitIdFieldDom.innerText = "";
}

class App {
    find_children(commitId) {
        var index = this.#commits.findIndex(e => e.id == commitId);
        if (index < -1) {
            return [];
        }

        var children = [];
        this.#commits.forEach(e => {
            if (e.parentIndexes && e.parentIndexes.indexOf(index) >= 0) {
                children.push(e);
            }
        });
        return children;
    }

    select_commit(commitId) {
        // clean last selection.
        if (this.#last_selected_row) {
            this.#last_selected_row.classList.remove("selected");
            this.#selectIndex = 0;
        }

        if (commitId) {
            this.#selectIndex = this.#commits.findIndex(c => c.id == commitId) + 1;
            var commitRow = document.getElementById(`commit-${commitId}`);
            if (commitRow) {
                commitRow.scrollIntoView({ "block": "nearest" });

                commitRow.classList.add("selected");
                this.#last_selected_row = commitRow;
                this.#load_commit_async(commitId);
            }
        }
        this.#update_selection_status();
    }

    async load_commits_async() {
        // clear old data.
        this.#commits = [];
        this.select_commit(null);
        const commitsListDom = document.getElementById("gitk-history-content");
        commitsListDom.replaceChildren();
        clean_commit_detail();

        show_commits_loading_wrapper(true);

        try {
            var url = `${server}/api/git-log?repo=${encodeURI(g_repo)}&path=${encodeURI(g_path)}`;
            if (g_noMergesCheckbox.checked) {
                url += "&noMerges=1";
            }
            if (g_commitId) {
                url += `&commit=${g_commitId}`;
            }
            if (g_author) {
                url += `&author=${g_author}`;
            }
            commitsListDom.replaceChildren();
            commitsListDom.classList.add("in-progress");

            const evtSource = new EventSource(url);
            const progressStatus = document.getElementById("progress-column");
            progressStatus.classList.remove("hidden");

            const closeEventSource = () => {
                evtSource.close();
                progressStatus.classList.add("hidden");
                commitsListDom.classList.remove("in-progress");
            };

            evtSource.onmessage = (e) => {
                const data = JSON.parse(e.data);
                const commits = data.commits;
                if (commits.length == 0) {
                    closeEventSource();
                    return;
                }

                this.#commits.push(...commits);
                for (var i = 0; i < data.graphs.length; ++i) {
                    this.#commits[i] = { ...this.#commits[i], ...data.graphs[i] };
                }
                for (var i = 0; i < commits.length; ++i) {
                    const commit = commits[i];
                    const row = document.createElement("div");
                    row.setAttribute("id", `commit-${commit.id}`);
                    row.setAttribute("style", `height: ${kLineHight}px`);
                    row.setAttribute("commitid", commit.id);
                    row.addEventListener("click", () => this.select_commit(commit.id));
                    row.commit = commit;
                    row.classList.add("row");

                    const graphAndMessage = document.createElement("div");
                    graphAndMessage.classList.add("graph-and-message");
                    const graph = document.createElement("div");
                    graph.setAttribute("id", `commit-${commit.id}-graph`);
                    graphAndMessage.appendChild(graph);
                    graphAndMessage.appendChild(crate_message(commit));
                    row.appendChild(graphAndMessage);

                    const author = document.createElement("div");
                    author.classList.add("author");
                    author.classList.add("oneline");
                    var txt = document.createTextNode(commit["author"]);
                    author.appendChild(txt);
                    row.appendChild(author);

                    const date = document.createElement("div");
                    date.classList.add("date");
                    txt = document.createTextNode(commit["date"]);
                    date.appendChild(txt);
                    row.appendChild(date);
                    commitsListDom.appendChild(row);
                }

                // Create graph data.
                for (var i = 0; i < this.#commits.length; ++i) {
                    var commit = this.#commits[i];
                    var graphDiv = document.getElementById(`commit-${commit.id}-graph`);
                    graphDiv.replaceChildren(crate_graph(commit, this.#commits));
                }

                this.#update_selection_status();
            }
            evtSource.onerror = (e) => {
                closeEventSource();
            }
        } catch (ex) {
            console.error(ex);
        }

        show_commits_loading_wrapper(false);
    }

    async #load_commit_async(commitId) {
        // clear old data.
        clean_commit_detail();

        const detailPanelDom = document.getElementById("commit-detail");
        const fileListDom = document.getElementById("commit-file-list");
        const commitIdFieldDom = document.getElementById("commit-id-field");

        show_detail_loading_wrapper(true);
        try {
            var url = `${server}/api/git-commit/${commitId}?repo=${encodeURI(g_repo)}&path=${encodeURI(g_path)}`;
            if (g_ignoreWhitespaceCheckbox.checked) {
                url += "&ignoreWhitespace=1";
            }
            const response = await fetch(url);
            const commit = await response.json();
            create_commit_detail(commit, detailPanelDom, fileListDom);
            commitIdFieldDom.innerText = commit.id;
        } catch (ex) {
            console.error(ex);
        }
        show_detail_loading_wrapper(false);
    }

    #update_selection_status() {
        document.getElementById("selection-column").innerText = `${this.#selectIndex} / ${this.#commits.length}`;
    }

    #commits = [];
    #selectIndex = 0;
    #last_selected_row;
}