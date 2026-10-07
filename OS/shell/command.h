#ifndef HOMEOS_SHELL_COMMAND_H
#define HOMEOS_SHELL_COMMAND_H

struct shell_command {
    const char *name;
    const char *help;
    void (*run)(int argc, char **argv);
};

/*
 * Un comando per file.
 *
 * Ogni sorgente in shell/cmds/ definisce il proprio comando con questa macro,
 * che dichiara la struttura (visibile alla tabella in shell.c) e la funzione.
 *
 * La registrazione e' una tabella esplicita in shell.c: la registrazione
 * automatica via sezione linker e' stata tentata ma con i nostri flag di link
 * (--gc-sections + --emit-relocs) i simboli __start_/__stop_ non vengono
 * generati. La tabella costa una riga per comando ed e' verificabile a occhio.
 */
#define SHELL_COMMAND(symbol, cmd_name, cmd_help)                        \
    static void symbol##_main(int argc, char **argv);                    \
    const struct shell_command symbol                                    \
        = { cmd_name, cmd_help, symbol##_main };                         \
    static void symbol##_main(int argc, char **argv)

#endif
