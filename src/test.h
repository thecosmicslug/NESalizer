// Automatic verification of test ROMs

void run_tests();
void report_status_and_end_test(uint8_t status, char const *msg);

extern char *testlist_filename;
extern bool end_testing;
