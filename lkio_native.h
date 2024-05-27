
// return 0 on OK, -1 on error
int word_in(int *word);

// return 0 on OK, -1 on error
int word_out(int word);

// return 0 on OK, -1 on error
int chan_in(char *, unsigned int count);

// return 0 on OK, -1 on error
int chan_out(char *, unsigned int count);

void rst_adpt(void);

// return 0 on OK, -1 on error
int  init_lkio();

// native builds only
void *get_host_in(void);
void *get_host_out(void);
