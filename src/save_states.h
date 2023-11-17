//* Save state and rewinding implementation

void init_save_states_for_rom();
void deinit_save_states_for_rom();

bool save_state(char const *statefile);
bool load_state(char const *statefile);