extern char userNames[256][100];
extern AccountUid userAccounts[10];
extern int selectedUser;

int push();
int pull();

void drawBorder();
void drawTabs(int selected);
void drawAppMenu();
void clearSelectedUser();