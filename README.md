# NX Save Sync

NX Save Sync is a switch homebrew app to quickly sync save files between switch and an emulator or between multiple modded consoles.

## Usage

### Sync from switch to pc

- Run switch app, press ZL / ZR or L / R to change the current user and select "Start server" in Send tab
- Move between pages with any left / any right and select the title you wand to send:
  - Single title with A
  - Multiple titles with Y (select / deselect) and A to start sending (not including the title thats hovered) 
  - All titles with X
- If no errors occured, you should see switch IP in yellow
- Open pc app, go to Config tab and set the switch IP - **Only for the first time when syncing from switch to pc**
- Go to Receive tab and select "Connect to switch"
- Paste the emulator save file directory and press enter (**the directory contents will be deleted!**) - **Only for the first time syncing the selected title**
- If no errors occured, the save file should be sent from switch to pc

### Sync from pc to switch

- Open pc app, go to Send tab and select "Start server"
- Select the title(s) or all titles you want to send
- If no errors occured, you should see pc IP
- Run switch app, go to Config tab and select "Set PC IP" and enter the pc IP. - **Only for the first time when syncing from pc to switch**
- Go to Receive tab and select "Connect to PC"
- If no errors occured, the save file should be sent from pc to switch

### Sync from primary switch to secondary switch

- Run app on primary switch, go to Send tab and select "Start server"
- Select the title(s) you want to send
- If no errors occured, you should see switch IP in yellow
- Run app on secondary switch, go to Config tab and set the secondary switch IP
- Go to Receive tab and select "Connect to secondary switch"
- If no errors occured, the save file should be sent from primary switch to secondary switch

The same procces is for syncing from secondary switch to primary switch

Note: During sync between switch and pc, both devices must be on the same network.

## Screenshots

<table width="100%">
  <thead>
    <tr>
      <th width="50%">Switch app</th>
      <th width="50%">Desktop app</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td width="50%"><img src="screenshots/switch1.jpg"/></td>
      <td width="50%"><img src="screenshots/desk1.png"/></td>
    </tr>
    <tr>
      <td width="50%"><img src="screenshots/switch2.jpg"/></td>
      <td width="50%"><img src="screenshots/desk2.png"/></td>
    </tr>
  </tbody>
</table>

## Build

```
git clone https://github.com/Xc987/NX-Save-Sync
cd NX-Save-Sync
```

### build switch app

```
cd switch
make
```

### build pc app (windows)

```
cd desktop
pip install -r requirements.txt
cxfreeze build
```

### build pc app (linux)

```
cd desktop
mkdir -p "build" "dist"
python3 -m venv build/venv
source build/venv/bin/activate
pip install dearpygui requests pyinstaller
pip install --only-binary=:all: pynput
pip install git+https://github.com/maddinpsy/pynput.git@fixup-xorg-merge-artifact
pyinstaller --windowed --name "NX-Save-Sync" \
  --add-data="$(python -c 'import dearpygui; print(dearpygui.__path__[0])'):dearpygui" \
  --add-data="$(python -c 'import pynput; print(pynput.__path__[0])'):pynput" \
  --hidden-import="pynput.keyboard._xorg" \
  --add-data="include:include" \
  --icon=include/icon.ico \
  main.py
mv $PWD/dist/NX-Save-Sync/_internal/include/ $PWD/dist/NX-Save-Sync/
```