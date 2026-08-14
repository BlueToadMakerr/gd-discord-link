<?php
require_once 'config.php';

$account_id = filter_input(INPUT_GET, 'account_id', FILTER_VALIDATE_INT);
$authtoken = filter_input(INPUT_GET, 'authtoken', FILTER_DEFAULT);

if (!$account_id || !$authtoken) {
    header("Location: fail.html?reason=missing_argon_credentials");
    exit;
}

// Verify token with Argon API
$argon_query = http_build_query([
    'account_id' => $account_id,
    'authtoken' => $authtoken
]);

$ch = curl_init(ARGON_API_URL . '?' . $argon_query);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_TIMEOUT, 10);
$response = curl_exec($ch);
$http_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
curl_close($ch);

if ($http_code !== 200 || !$response) {
    header("Location: fail.html?reason=argon_server_error");
    exit;
}

$argon_data = json_decode($response, true);

if (!isset($argon_data['valid']) || $argon_data['valid'] !== true) {
    $cause = isset($argon_data['cause']) ? urlencode($argon_data['cause']) : "invalid_argon_token";
    header("Location: fail.html?reason=" . $cause);
    exit;
}

// Create an HMAC-signed state parameter
$state_data = [
    'account_id' => $account_id,
    'timestamp' => time(),
    'authtoken' => $authtoken
];
$payload = json_encode($state_data);
$signature = hash_hmac('sha256', $payload, STATE_SECRET_KEY);
$state = base64_encode($payload . '|' . $signature);

// Redirect to Discord OAuth Authorization Page
$discord_oauth_url = "https://discord.com/api/oauth2/authorize?" . http_build_query([
    'client_id' => DISCORD_CLIENT_ID,
    'redirect_uri' => DISCORD_REDIRECT_URI,
    'response_type' => 'code',
    'scope' => 'identify guilds.join',
    'state' => $state
]);

header("Location: " . $discord_oauth_url);
exit;
?>