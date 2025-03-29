extern char userNames[256][100];
extern AccountUid userAccounts[10];
extern int selectedUser;
extern s32 total_users;

int push();
int pull();

void drawBorder();
void drawAppMenu();
void clearSelectedUser();