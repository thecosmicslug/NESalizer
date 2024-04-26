// Automatic verification of test ROMs

void setup_tests(char const *testlist);
void run_tests();
void report_status_and_end_test(uint8_t status, char const *msg);
int CountTestList();

extern char const *testlist_filename;
extern bool end_testing;

extern int TotalTests;
extern int CurrentTestNum;