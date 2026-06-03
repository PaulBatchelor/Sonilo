
/* Word machine location is right after
 * Sonilo.
 * (2^16 words) / (256 words/megablock) = index 256
 */

#define WM_LOCATION 256

typedef struct word_machine word_machine;

void word_machine_init(word_machine *wm, uint16_t vm);

/* send: send byte to word machine */
int word_machine_send(word_machine *wm, char c);

/* begin: begin a new message */
int word_machine_begin(word_machine *wm);

/* end: parse bytes in buffer and clear message */
int word_machine_end(word_machine *wm);
