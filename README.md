#  Discord RPC Linker
<img src="logo.png" width="150" alt="ITS THE LITERAL FUCKING LOGOOOOOOO JIOEGOIUGYIUODHOEIUHDPIOEHD"/>

Show off your Rich Presence status from Discord on your Geometry Dash profile!
### Status colors
- Green: User is online and has a presence enabled
- Grey: User is offline, or isn't sharing any presence data
- Pink: (Only displays on your profile) User isn't linked
> Note: Only people that also have this mod can see this. If the user doesn't have the mod installed, the status indicator will not show up.

How we handle your data can be seen at our [Privacy Policy](https://bluetoadmaker.infinityfreeapp.com/discordgdlinker/privacy.html)

---

This mod works great in pair with the following mod: [Discord Rich Presence](https://geode-sdk.org/mods/techstudent10.discord_rich_presence).

## Self hosting
Self hostng files can be found in the backend folder in this repository :3<br>
*This guide is so terribly written.. Just dm me on Discord at BlueToadMaker if you have any questions* TwT

### Firebase setup
First, set up a Firebase Realtime Database. I won't bore you with the creating stuff but just know you will need your Firebase Certificate file for the web server and python. Also in the database create an entry called `oauth_url`. This will be used later for the oauth url in Geometry Dash. After that, go into rules and paste this in
```json
{
  "rules": {
    "users": {
      "$account_id": {
        ".read": true,
        ".write": false
      }
    },
    "oauth_errors": {
      "$account_id": {
        ".read": true,
        ".write": false
      }
    },
    "oauth_queue": {
      ".read": false,
      ".write": false
    },
    "user_data": {
      "$uid": {
        ".read": true,
        ".write": "data.exists()",
        "discord_id": {
      		".validate": "newData.val() === data.val()"
    		},
    		"account_id": {
      		".validate": "newData.val() === data.val()"
    		}
      }
    },
    "auth_tokens": {
      "$authtoken": {
        ".read": true,
        ".write": "data.exists()"
      },
    },
    "oauth_url": {
      ".read": true,
      ".write": false
    }
  }
}
```
This is to secure the data in Firebase. This isn't a required step.. but.. I **highly** recommend you do.

### Discord setup
Create a discord server and copy its Guild ID (If you don't know how to, look it up), this will be used for later. Next create a discord bot at the Discord Developer Portal. Enable Presence Intent and Server Members Intent in the Bot tab, then, in the OAuth2 tab, Go into the OAuth2 genorator. Enable Bot and in Bot Permissions, enable the permission to Kick Members. You will also need to copy the Client ID, Secret, and Token.

### Web
You will need this running on a PHP runnable web server. This is to manage redirects from discord and for Geometry Dash (In my case, I used Infinityfree.) Copy all of the files in the web folder and place them somewhere. Get your Firebase Service Certificate and place it in the same folder. After that, go into the `config.php` file and fill everything in! The web server is now ready ^w^ Make sure to set the redirect URI in discord to server.com/auth.php and the `oauth_url` in Firebase to server.com/init.py and now you're ready for the actual server setup!

#### config.php
```php
// Discord Bot
define('DISCORD_CLIENT_ID', 'BOT_CLIENT_ID');
define('DISCORD_CLIENT_SECRET', 'SECRET');
define('DISCORD_BOT_TOKEN', 'TOKEN');
define('DISCORD_GUILD_ID', 'SERVER THE BOT WILL ALSO BE IN');
define('DISCORD_REDIRECT_URI', 'REDIRECT URI (SET IN DISCORD)');

// Firebase Realtime Database
define('FIREBASE_DB_URL', 'Firebase DB link');
define('SERVICE_ACCOUNT_PATH', __DIR__ . '/path to the service creds');

// Argon API
define('ARGON_API_URL', 'Argon | For example: https://argon.globed.dev/v1/validation/check');

// Secret Key for signing state tokens
define('STATE_SECRET_KEY', 'Just random spam lol. Used to sign state keys to make sure they are not tampered with');
```

### Python
This can run on literally anything that can run Python and has internet. I am kinda lazy to see what you need to install so uhhhhh idk look in the file lol :3 You will need your Firebase Service Certificate in the same directory as `main.py`<br>
Also make sure to config the `.env` file with everything :3

#### .env
```UwU
CLIENT_ID=Discord bot client ID
CLIENT_SECRET=Discord bot client secret
REDIRECT_URI=Redirect URI (Same as in Discord)
TARGET_GUILD_ID=Server the bot will also be in
FIREBASE_CERT_DIR=Service worker certificate file name
DATABASE_URL=Firebase Database url
DISCORD_BOT_TOKEN=Discord bot token
```

## You're done!
To test, go in the mod's settings, and edit the Firebase Database URL to your Firebase URL!