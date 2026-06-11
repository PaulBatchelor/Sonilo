
/* Word machine location is right after
 * Sonilo.
 * (2^16 words) / (256 words/megablock) = index 256
 */

#define WM_LOCATION 256

typedef struct word_machine word_machine;

void word_machine_init(word_machine *wm, uint16_t vm);

/* send: send byte to word machine */
int word_machine_send(word_machine *wm, unsigned char c);

/* begin: begin a new message */
int word_machine_begin(word_machine *wm);

/* end: parse bytes in buffer and clear message */
int word_machine_end(word_machine *wm);

/* wpos: get current position in word */
int word_machine_wpos(word_machine *wm);

/* bpos: get current position in buffer */
int word_machine_bpos(word_machine *wm);

/* clear: clear word buffer */
void word_machine_clear(word_machine *wm);

/* word: get current word machine state */
uint32_t word_machine_word(word_machine *wm);

/* check if word is system command */
int word_machine_issys(uint32_t w);

int word_machine_extract_sys(uint32_t w, uint8_t *k, uint8_t *v);

/* append: appends word to the word buffer */
int word_machine_append(word_machine *wm, uint32_t w);

/* append_half: appends half-word to the word buffer */
int word_machine_append_half(word_machine *wm, uint16_t hw, int which);
