<?php
require_once 'config.php';
require_once 'firebase_auth.php';

// Check for Discord error response
if (isset($_GET['error'])) {
    header("Location: fail.html?reason=" . urlencode($_GET['error']));
    exit;
}

$code = $_GET['code'] ?? null;
$state = $_GET['state'] ?? null;

if (!$code || !$state) {
    header("Location: fail.html?reason=missing_oauth_parameters");
    exit;
}

// Validate state parameter
$decoded = base64_decode($state);
$last_delimiter = strrpos($decoded, '|');

if ($last_delimiter === false) {
    header("Location: fail.html?reason=invalid_state_format");
    exit;
}

$payload = substr($decoded, 0, $last_delimiter);
$signature = substr($decoded, $last_delimiter + 1);

// Verify HMAC signature
$expected_signature = hash_hmac('sha256', $payload, STATE_SECRET_KEY);

if (!hash_equals($expected_signature, $signature)) {
    header("Location: fail.html?reason=state_signature_mismatch");
    exit;
}

// Verify state data
$state_data = json_decode($payload, true);
if (!$state_data || !isset($state_data['account_id'], $state_data['timestamp'], $state_data['authtoken'])) {
    header("Location: fail.html?reason=invalid_state_data");
    exit;
}

$gd_account_id = $state_data['account_id'];
$timestamp = $state_data['timestamp'];
$authtoken = $state_data['authtoken'];

// Expire state after 10 minutes
if (time() - (int)$timestamp > 600) {
    header("Location: fail.html?reason=session_expired");
    exit;
}

// Sanitize authtoken for Firebase
$safe_token = str_replace(['.', '#', '$', '[', ']'], '_', $authtoken);

// Get Google OAuth Token
$fb_token = getFirebaseAccessToken(SERVICE_ACCOUNT_PATH);

if (!$fb_token) {
    header("Location: fail.html?reason=firebase_token_generation_failed");
    exit;
}

// Push OAuth code to Firebase Queue
// We do this as Infinityfree blocks requests made to Discord.
// So its done on the server side instead
$queue_url = FIREBASE_DB_URL . "oauth_queue/" . $gd_account_id . ".json";
$queue_payload = json_encode([
    'code' => $code,
    'authtoken' => $authtoken,
    'status' => 'pending',
    'timestamp' => time()
]);

$ch = curl_init($queue_url);
curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "PUT");
curl_setopt($ch, CURLOPT_POSTFIELDS, $queue_payload);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_HTTPHEADER, [
    "Authorization: Bearer " . $fb_token,
    "Content-Type: application/json"
]);

$fb_response = curl_exec($ch);
$fb_http_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
curl_close($ch);

if ($fb_http_code !== 200 || !$fb_response) {
    header("Location: fail.html?reason=queue_write_failed");
    exit;
}
?>
<!DOCTYPE html>
<html>
<head>
    <title>Linking Discord...</title>
</head>
<body>
    <h1>Linking your Discord account...</h1>
    <p>Please wait while our server registers your account. This may take a while...</p>

    <script>
        const accountId = "<?php echo $gd_account_id; ?>";
        const safeToken = "<?php echo $safe_token; ?>";
        const dbUrl = "<?php echo FIREBASE_DB_URL; ?>";
        const timeoutHandle = setTimeout(() => {
            clearInterval(checkInterval);
            window.location.href = "fail.html?reason=no_response_from_server";
        }, 10000);

        const checkInterval = setInterval(async () => {
            try {
                let tokenRes = await fetch(`${dbUrl}auth_tokens/${safeToken}.json`);
                if (tokenRes.ok) {
                    let tokenData = await tokenRes.json();
                    if (tokenData && tokenData.uid) {
                        clearTimeout(timeoutHandle);
                        clearInterval(checkInterval);
                        window.location.href = "success.html";
                        return;
                    }
                }
                let errRes = await fetch(`${dbUrl}oauth_errors/${accountId}.json`);
                if (errRes.ok) {
                    let errData = await errRes.json();
                    if (errData && errData.error) {
                        clearTimeout(timeoutHandle);
                        clearInterval(checkInterval);
                        window.location.href = `fail.html?reason=${encodeURIComponent(errData.error)}`;
                        return;
                    }
                }
            } catch (e) {
                console.error("Polling error:", e);
            }
        }, 1500);
    </script>
</body>
</html>