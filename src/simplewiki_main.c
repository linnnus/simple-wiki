#include "arena.h"
#include "die.h"
#include "strutil.h"
#include "creole.h"

// #include <assert.h>
#include <errno.h>     // errno, EEXIST
#include <git2.h>      // git_*
#include <unistd.h>    // link
#include <stdbool.h>   // false
#include <stdio.h>
#include <stdlib.h>    // EXIT_SUCCESS
#include <sys/stat.h>  // mkdir
#include <sys/types.h> // mode_t

#define REF "refs/heads/master"

void xmkdir(const char *path, mode_t mode, bool exist_ok) {
	if (mkdir(path, mode) < 0) {
		if (exist_ok && errno == EEXIST) {
			return;
		} else {
			die_errno("failed to mkdir %s", path);
		}
	}
}

void xsymlink(const char *source_path, const char *target_path)
{
	if (symlink(source_path, target_path) < 0) {
		die_errno("failed to link '%s' => '%s'", target_path, source_path);
	}
}

void process_other_file(const char *path, const char *source, size_t source_len) {
	FILE *out = fopen(path, "w");
	printf("Copying: %s\n", path);
	if (out == NULL) {
		die_errno("failed to open %s for writing", path);
	}
	if (fwrite(source, 1, source_len, out) < source_len) {
		die_errno("failed to write content to %s\n", path);
	}
	fclose(out);
}

void process_markup_file(struct arena *a, const char *path, const char *source, size_t source_len) {
	char *out_path = replace_suffix(a, path, ".txt", ".html");
	printf("Generating: %s\n", out_path);
	FILE *out = fopen(out_path, "w");
	if (out == NULL) {
		die_errno("failed to open %s for writing", path);
	}
	render_creole(out, source, source_len);
	fclose(out);
}

void process_dir(const char *path) {
	xmkdir(path, 0755, false);
}

void list_tree(struct arena *a, struct git_repository *repo, struct git_tree *tree, const char *prefix) {
	// Grab a snapshot of the arena.
	// All memory allocated within the arena in this subcalltree will be freed.
	// This is effectively the same as allocating a new arena for each call to list_tree.
	struct arena snapshot = *a;

	size_t tree_count = git_tree_entrycount(tree);
	for (size_t i = 0; i < tree_count; ++i) {
		// Read the entry.
		const struct git_tree_entry *entry;
		if ((entry = git_tree_entry_byindex(tree, i)) == NULL) {
			die("read tree item");
		}

		// Construct path to entry.
		const char *entry_out_path = joinpath(a, prefix, git_tree_entry_name(entry));

		// entry->obj fail on submodules. just ignore them.
		struct git_object *obj;
		if (git_tree_entry_to_object(&obj, repo, entry) == 0) {
			git_object_t type = git_object_type(obj);
			switch (type) {
				case GIT_OBJECT_BLOB: {
					struct git_blob *blob = (struct git_blob *)obj;
					const char *source = git_blob_rawcontent(blob);
					if (source == NULL) {
						die_git("get source for blob %s", git_oid_tostr_s(git_object_id(obj)));
					}
					size_t source_len = git_blob_rawsize(blob);
					if (endswith(entry_out_path, ".txt") && !git_blob_is_binary(blob)) {
						process_markup_file(a, entry_out_path, source, source_len);
					} else {
						process_other_file(entry_out_path, source, source_len);
					}
					git_object_free(obj);
				} break;
				case GIT_OBJECT_TREE: {
					process_dir(entry_out_path);
					list_tree(a, repo, (struct git_tree *)obj, entry_out_path);
					git_object_free(obj);
				} break;
				default: {
					// Ignore whatever weird thing this is.
					git_object_free(obj);
				} break;
			}
		}
	}

	// Restore snapshot.
	*a = snapshot;
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		die("Usage: %s git-path out-path", argv[0]);
	}
	char *git_path = argv[1];
	char *out_path = argv[2];

        // Initialize libgit. Note that calling git_libgit2_shutdown is not
        // necessary, as per this snippet from the documentation:
        //
        // > Usually you don’t need to call the shutdown function as the operating
        // > system will take care of reclaiming resources, but if your
        // > application uses libgit2 in some areas which are not usually active,
        // > you can use
	//
	// That's good news!
        if (git_libgit2_init() < 0) {
		die_git("initialize libgit");
	}

	// Do not search outside the git repository. GIT_CONFIG_LEVEL_APP is the highest level currently.
	// for (int i = 1; i <= GIT_CONFIG_LEVEL_APP; i++) {
	// 	if (git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, i, "") < 0) {
	// 		die_git("set search path");
	// 	}
	// }

	// Don't require the repository to be owned by the current user.
	git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);

	struct git_repository *repo;
	if (git_repository_open_ext(&repo, git_path, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) < 0) {
		die_git("open repository");
	}

	// Create a revision walker to iterate commits on the main branch.
	git_revwalk *walker = NULL;
	git_revwalk_new(&walker, repo);
	git_revwalk_push_ref(walker, REF);

	// Create the initial output directory.
	xmkdir(out_path, 0755, true);

	struct arena a = arena_create(2048);
	git_oid commit_oid;
	while (git_revwalk_next(&commit_oid, walker) == 0) {
		const char *commit_sha = git_oid_tostr_s(&commit_oid);

		git_commit *commit = NULL;
		if (git_commit_lookup(&commit, repo, &commit_oid) < 0) {
			die_git("find commit %s", commit_sha);
		}

		struct git_tree *tree;
		if (git_commit_tree(&tree, commit) < 0) {
			die_git("get tree for commit %s", commit_sha);
		}

		const char *prefix = joinpath(&a, out_path, commit_sha);
		xmkdir(prefix, 0755, true);
		list_tree(&a, repo, tree, prefix);

		a.used = 0; // reset arena after each iteration
		git_commit_free(commit);
		git_tree_free(tree);
	}

	// Create a symbolic link to the latest commit.
	git_oid latest_commit;
	if (git_reference_name_to_id(&latest_commit, repo, REF) < 0) {
		die_git("Failed to resolve " REF " to commit");
	}
	const char *source = git_oid_tostr_s(&latest_commit);
	const char *target = joinpath(&a, out_path, "latest");
	xsymlink(source, target);

#ifndef NDEBUG
	arena_destroy(&a);
#endif

#ifndef NDEBUG
	git_libgit2_shutdown();
#endif
        return EXIT_SUCCESS;
}
