<?php
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
define('ARGON_API_URL', 'Argon API | For example: https://argon.globed.dev/v1/validation/check');

// Secret Key for signing state tokens
define('STATE_SECRET_KEY', 'Just random spam lol. Used to sign state keys to make sure they are not tampered with (MAKE SURE THIS STAYS A SECRET)');
?>