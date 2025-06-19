module;

#include <memory>
#include <thirdparty/libgit2/include/git2.h>

export module gitkf:git_smart_pointer;

export template <>
struct std::default_delete<git_buf> {
    void operator()(git_buf* p) const { git_buf_dispose(p); }
};

export template <>
struct std::default_delete<git_commit> {
    void operator()(git_commit* p) const { git_commit_free(p); }
};

export template <>
struct std::default_delete<git_diff> {
    void operator()(git_diff* p) const { git_diff_free(p); }
};

export template <>
struct std::default_delete<git_repository> {
    void operator()(git_repository* p) const { git_repository_free(p); }
};

export template <>
struct std::default_delete<git_tree> {
    void operator()(git_tree* p) const { git_tree_free(p); }
};
