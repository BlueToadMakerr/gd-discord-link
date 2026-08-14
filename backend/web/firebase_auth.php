<?php
function base64UrlEncode($data) {
    return rtrim(strtr(base64_encode($data), '+/', '-_'), '=');
}

function getFirebaseAccessToken($serviceAccountPath) {
    if (!file_exists($serviceAccountPath)) {
        return false;
    }

    $sa = json_decode(file_get_contents($serviceAccountPath), true);
    if (!$sa || !isset($sa['private_key']) || !isset($sa['client_email'])) {
        return false;
    }

    // Build JWT Header & Payload
    $header = base64UrlEncode(json_encode(['alg' => 'RS256', 'typ' => 'JWT']));
    $now = time();
    $payload = base64UrlEncode(json_encode([
        'iss' => $sa['client_email'],
        'scope' => 'https://www.googleapis.com/auth/firebase.database https://www.googleapis.com/auth/userinfo.email',
        'aud' => 'https://oauth2.googleapis.com/token',
        'exp' => $now + 3600,
        'iat' => $now
    ]));

    $baseString = $header . '.' . $payload;

    // Sign JWT using our private key
    $signature = '';
    $success = openssl_sign($baseString, $signature, $sa['private_key'], OPENSSL_ALGO_SHA256);
    if (!$success) {
        return false;
    }

    $jwt = $baseString . '.' . base64UrlEncode($signature);

    // Grab the Google Access Token
    $ch = curl_init('https://oauth2.googleapis.com/token');
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($ch, CURLOPT_POST, true);
    curl_setopt($ch, CURLOPT_POSTFIELDS, http_build_query([
        'grant_type' => 'urn:ietf:params:oauth:grant-type:jwt-bearer',
        'assertion' => $jwt
    ]));

    $response = json_decode(curl_exec($ch), true);
    curl_close($ch);

    return $response['access_token'] ?? false;
}
?>