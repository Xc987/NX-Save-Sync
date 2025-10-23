extern char userNames[256][100];
extern AccountUid userAccounts[12];
extern int selectedUser;
extern s32 total_users;

int push();
int pull(int device);

void drawBorder();
void drawTabs(int selected);
void drawAppMenu();
void clearSelectedUser();
void drawTempZipWarning();