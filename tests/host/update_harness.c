#define ensure_dir patch_db_ensure_dir
#include "../../source/patch_db.c"
#undef ensure_dir
#include "../../source/repo_update.c"

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "active") == 0) {
        puts(active_pipe_db_path(argv[2], argv[3]));
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "download") == 0) {
        return download_http_file(argv[2], argv[3]) == 0 ? 0 : 1;
    }
    if (argc == 2 && strcmp(argv[1], "urls") == 0) {
        char url[1200];
        int i;
        for (i = 0; i < 2; ++i) {
            if (build_file_url(url, sizeof(url), "http://example.com/", "patches.csv") != 0) {
                return 1;
            }
            puts(url);
        }
        return 0;
    }
    return 2;
}
