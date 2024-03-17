
/*
 *  Function declarations for the Link I/O interface to the link adaptor.
 *  Copyright (C) Computer System Architects, 1988
 */


// return 0 on OK, -1 on error
int tbyte_out(int);

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
