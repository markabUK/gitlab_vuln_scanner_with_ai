#pragma once
#include <git2.h>
#include <string>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <unordered_set>
#include <mutex>

class GitManager {
private:
    git_repository* repo = nullptr;
    std::filesystem::path localPath;
    std::string token;
    bool keepWorkspace = false;

    inline static std::unordered_set<std::string> activeWorkspaces;
    inline static std::mutex workspaceMutex;

    static int CredentialsCallback(git_cred** out, const char*, const char*, unsigned int, void* payload) {
        return git_cred_userpass_plaintext_new(out, "oauth2", static_cast<std::string*>(payload)->c_str());
    }

public:
    static void CleanupAllWorkspaces() {
        std::lock_guard<std::mutex> lock(workspaceMutex);
        for (const auto& path : activeWorkspaces) {
            std::error_code ec;
            if (std::filesystem::exists(path)) {
                std::filesystem::remove_all(path, ec);
                spdlog::info("Cleaned up orphaned workspace: {}", path);
            }
        }
        activeWorkspaces.clear();
    }

    GitManager(const std::string& gitlabToken) : token(gitlabToken) {
        git_libgit2_init();
    }

    ~GitManager() {
        if (repo) git_repository_free(repo);
        git_libgit2_shutdown();
        
        if (!keepWorkspace) {
            std::error_code ec;
            if (std::filesystem::exists(localPath)) {
                std::filesystem::remove_all(localPath, ec);
            }
        }

        std::lock_guard<std::mutex> lock(workspaceMutex);
        activeWorkspaces.erase(localPath.string());
    }

    void SetKeepWorkspace(bool keep) { 
        keepWorkspace = keep; 
    }

    bool Clone(const std::string& repoUrl, const std::string& projectId) {
        localPath = std::filesystem::temp_directory_path() / ("updater_proj_" + projectId);
        std::error_code ec;
        if (std::filesystem::exists(localPath)) std::filesystem::remove_all(localPath, ec);

        // Register the path so Ctrl+C knows about it
        {
            std::lock_guard<std::mutex> lock(workspaceMutex);
            activeWorkspaces.insert(localPath.string());
        }

        git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
        clone_opts.fetch_opts.callbacks.credentials = CredentialsCallback;
        clone_opts.fetch_opts.callbacks.payload = &token;

        spdlog::info("[Git] Preparing local analysis workspace...");
        int err = git_clone(&repo, repoUrl.c_str(), localPath.string().c_str(), &clone_opts);
        if (err != 0) {
            const git_error* e = git_error_last();
            spdlog::error("[Git] Clone failed: {}", e ? e->message : "Unknown error");
        }
        return err == 0;
    }

    bool CreateBranch(const std::string& branchName) {
        git_object* head_commit = nullptr;
        git_reference* branch = nullptr;
        git_revparse_single(&head_commit, repo, "HEAD");
        
        int err = git_branch_create(&branch, repo, branchName.c_str(), (git_commit*)head_commit, 0);
        git_object_free(head_commit);
        
        if (err == 0) git_repository_set_head(repo, git_reference_name(branch));
        git_reference_free(branch);
        return err == 0;
    }

    bool HasChanges() {
        git_status_list* status_list = nullptr;
        git_status_options status_opts = GIT_STATUS_OPTIONS_INIT;
        status_opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
        status_opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED;

        git_status_list_new(&status_list, repo, &status_opts);
        size_t count = git_status_list_entrycount(status_list);
        git_status_list_free(status_list);
        return count > 0;
    }

    bool CommitAll(const std::string& message) {
        git_index* index = nullptr;
        git_repository_index(&index, repo);
        git_index_add_all(index, nullptr, GIT_INDEX_ADD_DEFAULT, nullptr, nullptr);
        git_index_write(index);

        git_oid tree_id, commit_id;
        git_index_write_tree(&tree_id, index);
        git_tree* tree = nullptr;
        git_tree_lookup(&tree, repo, &tree_id);

        git_signature* me;
        git_signature_now(&me, "Dependency Bot", "bot@automation.local");

        git_reference* head_ref = nullptr;
        git_repository_head(&head_ref, repo);
        git_commit* parent = nullptr;
        git_reference_peel((git_object**)&parent, head_ref, GIT_OBJECT_COMMIT);

        int err = git_commit_create_v(&commit_id, repo, "HEAD", me, me, nullptr, message.c_str(), tree, 1, parent);

        git_commit_free(parent); git_reference_free(head_ref);
        git_signature_free(me); git_tree_free(tree); git_index_free(index);
        return err == 0;
    }

    bool Push(const std::string& branchName) {
        git_remote* remote = nullptr;
        git_remote_lookup(&remote, repo, "origin");

        git_push_options push_opts = GIT_PUSH_OPTIONS_INIT;
        push_opts.callbacks.credentials = CredentialsCallback;
        push_opts.callbacks.payload = &token;

        std::string ref = "refs/heads/" + branchName;
        char* ref_ptr = const_cast<char*>(ref.c_str());
        git_strarray refspecs = { &ref_ptr, 1 };

        spdlog::info("[Git] Pushing all local changes atomically to remote repository...");
        int err = git_remote_push(remote, &refspecs, &push_opts);
        git_remote_free(remote);
        return err == 0;
    }

    std::string GetLocalPath() const { return localPath.string(); }
};