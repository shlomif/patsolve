
#define FCS_PATS__ACE 1
#define FCS_PATS__CLUB 0x01    // black
#define FCS_PATS__DIAMOND 0x02 // red
#define FCS_PATS__HEART 0x00   // red
#define FCS_PATS__SPADE 0x03   // black

static inline fcs_card fcs_pats_card_color(const fcs_card card)
{
    return (card & FCS_PATS__COLOR);
}
