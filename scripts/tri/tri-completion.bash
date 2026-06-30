# bash/zsh completion for tri command
# Install: source this file or place in /etc/bash_completion.d/

_tri_complete() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    
    # Top-level commands
    local top_cmds="status balance peers stake address send tx msg raw help"
    local addr_subcmds="new list balance"
    local msg_subcmds="inbox outbox send anon keys enable pubkey unlock"
    
    if [[ ${COMP_CWORD} -eq 1 ]]; then
        COMPREPLY=($(compgen -W "${top_cmds}" -- "${cur}"))
        return 0
    fi
    
    # Subcommand completion
    if [[ ${COMP_CWORD} -eq 2 ]]; then
        case "${COMP_WORDS[1]}" in
            address|addr)
                COMPREPLY=($(compgen -W "${addr_subcmds}" -- "${cur}"))
                return 0
                ;;
            msg|message|messages)
                COMPREPLY=($(compgen -W "${msg_subcmds}" -- "${cur}"))
                return 0
                ;;
        esac
    fi
    
    # Address completion for send/msg send (would need wallet addresses in practice)
    # For now, no further completion
    
    return 0
}

complete -F _tri_complete tri
