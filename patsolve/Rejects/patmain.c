void print_move(MOVE *mp)
{
    fc_solve_pats__print_card(mp->card, stderr);
    if (mp->totype == T_TYPE)
    {
        print_msg("to temp (%d)\n", mp->pri);
    }
    else if (mp->totype == O_TYPE)
    {
        print_msg("out (%d)\n", mp->pri);
    }
    else
    {
        print_msg("to ");
        if (mp->destcard == NONE)
        {
            print_msg("empty pile (%d)", mp->pri);
        }
        else
        {
            fc_solve_pats__print_card(mp->destcard, stderr);
            print_msg("(%d)", mp->pri);
        }
        fputc('\n', stderr);
    }
    fc_solve_pats__print_layout(soft_thread);
}
