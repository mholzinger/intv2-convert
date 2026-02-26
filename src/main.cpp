#include <cstdio>
#include <cstring>
#include <string>

int cmd_rom  (int argc, char* argv[]);
int cmd_cfg  (int argc, char* argv[]);
int cmd_lst  (int argc, char* argv[]);
int cmd_batch(int argc, char* argv[]);

static void usage() {
    fprintf(stderr,
        "intv2convert -- Convert Intellivision ROMs to INTV2 format\n"
        "\n"
        "Usage:\n"
        "  intv2convert rom   <input.rom> <output_stem>\n"
        "  intv2convert cfg   <input.bin> <input.cfg> <output_stem>\n"
        "  intv2convert lst   <input.lst> <output.intv> [--pocket]\n"
        "  intv2convert batch <source_dir> <output_dir> [--dry-run] [--force]\n"
        "\n"
        "Outputs:\n"
        "  rom, cfg  Writes <output_stem>-nt-noir.intv and <output_stem>-pocket.intv\n"
        "  lst       Writes one INTV2 file; add --pocket for the Analogue Pocket variant\n"
        "  batch     Walks source_dir and converts all supported ROMs into output_dir\n"
    );
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "rom")   return cmd_rom  (argc - 1, argv + 1);
    if (cmd == "cfg")   return cmd_cfg  (argc - 1, argv + 1);
    if (cmd == "lst")   return cmd_lst  (argc - 1, argv + 1);
    if (cmd == "batch") return cmd_batch(argc - 1, argv + 1);

    fprintf(stderr, "Error: unknown subcommand '%s'\n\n", argv[1]);
    usage();
    return 1;
}
