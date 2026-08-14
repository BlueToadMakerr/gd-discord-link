import asyncio
import time
import json
import os
import aiohttp
import discord
import uuid
from discord.ext import tasks
import firebase_admin
from firebase_admin import credentials, db
from dotenv import load_dotenv
load_dotenv()

# --- CONFIG ---
CLIENT_ID = os.getenv("CLIENT_ID")
CLIENT_SECRET = os.getenv("CLIENT_SECRET")
REDIRECT_URI = os.getenv("REDIRECT_URI")
TARGET_GUILD_ID = os.getenv("TARGET_GUILD_ID")

# Initialize Firebase
cred = credentials.Certificate(os.getenv("FIREBASE_CERT_DIR"))
firebase_admin.initialize_app(cred, {
    'databaseURL': os.getenv("DATABASE_URL")
})

intents = discord.Intents.default()
intents.members = True    
intents.presences = True 
bot = discord.Client(intents=intents)

async def write_error_and_cleanup(account_id, error_reason):
    error_data = {
        'error': error_reason,
        'timestamp': int(time.time())
    }
    await asyncio.to_thread(db.reference(f"oauth_errors/{account_id}").set, error_data)
    await asyncio.to_thread(db.reference(f"oauth_queue/{account_id}").delete)
    
    async def delayed_delete():
        await asyncio.sleep(10)
        try:
            await asyncio.to_thread(db.reference(f"oauth_errors/{account_id}").delete)
        except Exception:
            pass
            
    asyncio.create_task(delayed_delete())

async def cleanup_auth_token(safe_token):
    await asyncio.sleep(60)
    try:
        await asyncio.to_thread(db.reference(f"auth_tokens/{safe_token}").delete)
    except Exception:
        pass

@bot.event
async def on_ready():
    print("[+] Bot is ready!")
    if not process_queue.is_running():
        process_queue.start()
    if not process_unlink_queue.is_running():
        process_unlink_queue.start()

@tasks.loop(seconds=3)
async def process_queue():
    try:
        queue_data = await asyncio.to_thread(db.reference("oauth_queue").get)
        if not queue_data:
            return

        async with aiohttp.ClientSession() as session:
            for account_id, data in queue_data.items():
                if data and data.get("status") == "pending":

                    code = data.get("code")
                    authtoken = data.get("authtoken")

                    if not code:
                        await write_error_and_cleanup(account_id, "missing_oauth_code")
                        continue

                    async with session.post("https://discord.com/api/oauth2/token", data={
                        'client_id': CLIENT_ID,
                        'client_secret': CLIENT_SECRET,
                        'grant_type': 'authorization_code',
                        'code': code,
                        'redirect_uri': REDIRECT_URI
                    }, headers={'Content-Type': 'application/x-www-form-urlencoded'}) as token_res:
                        
                        if token_res.status != 200:
                            err_text = await token_res.text()
                            await write_error_and_cleanup(account_id, "discord_token_exchange_failed")
                            continue
                        
                        token_data = await token_res.json()
                        access_token = token_data.get('access_token')

                    async with session.get("https://discord.com/api/users/@me", headers={
                        'Authorization': f'Bearer {access_token}'
                    }) as user_res:
                        
                        if user_res.status != 200:
                            await write_error_and_cleanup(account_id, "failed_fetching_discord_user")
                            continue
                        
                        discord_user = await user_res.json()
                        discord_id = str(discord_user.get('id'))

                    all_user_data = await asyncio.to_thread(db.reference("user_data").get)
                    
                    discord_conflict = False
                    existing_uid_for_gd = None
                    existing_discord_for_gd = None

                    if all_user_data:
                        for u_id, u_val in all_user_data.items():
                            if u_val:
                                stored_acc = str(u_val.get("account_id"))
                                stored_disc = str(u_val.get("discord_id"))
                                
                                if stored_disc == discord_id and stored_acc != str(account_id):
                                    discord_conflict = True
                                    break
                                
                                if stored_acc == str(account_id):
                                    existing_uid_for_gd = u_id
                                    existing_discord_for_gd = stored_disc

                    if discord_conflict:
                        await write_error_and_cleanup(account_id, "discord_already_linked")
                        continue

                    if existing_uid_for_gd:
                        if existing_discord_for_gd != discord_id:
                            await write_error_and_cleanup(account_id, "gd_already_linked")
                            continue
                        else:
                            if authtoken:
                                safe_token = authtoken.replace('.', '_').replace('#', '_').replace('$', '_').replace('[', '_').replace(']', '_')
                                await asyncio.to_thread(db.reference(f"auth_tokens/{safe_token}").set, {'uid': existing_uid_for_gd})
                                asyncio.create_task(cleanup_auth_token(safe_token))

                            await asyncio.to_thread(db.reference(f"oauth_queue/{account_id}").delete)
                            continue

                    async with session.put(f"https://discord.com/api/v10/guilds/{TARGET_GUILD_ID}/members/{discord_id}", 
                        json={'access_token': access_token}, 
                        headers={
                            "Authorization": f"Bot {bot.http.token}",
                            "Content-Type": "application/json"
                        }
                    ) as guild_res:
                        guild_status = guild_res.status
                        if guild_status not in [201, 204]:
                            guild_err = await guild_res.text()
                            
                            error_reason = "guild_join_failed"
                            if "30001" in guild_err:
                                error_reason = "discord_server_limit_reached"
                            if "50026" in guild_err:
                                error_reason = "guild_join_no_permission"
                            
                            await write_error_and_cleanup(account_id, error_reason)
                            continue

                    uid = uuid.uuid4().hex
                    private_data = {
                        'discord_id': str(discord_id),
                        'account_id': str(account_id),
                    }
                    await asyncio.to_thread(db.reference(f"user_data/{uid}").set, private_data)
                    new_user_data = {
                        'is_active': True,
                        'linked_at': int(time.time())
                    }
                    await asyncio.to_thread(db.reference(f"users/{account_id}").set, new_user_data)

                    if authtoken:
                        safe_token = authtoken.replace('.', '_').replace('#', '_').replace('$', '_').replace('[', '_').replace(']', '_')    
                        await asyncio.to_thread(db.reference(f"auth_tokens/{safe_token}").set, {'uid': uid})
                        asyncio.create_task(cleanup_auth_token(safe_token))
                    
                    print(f"[+] Successfully linked GD {account_id} to Discord {discord_id} (UID: {uid})")

                    await asyncio.to_thread(db.reference(f"oauth_queue/{account_id}").delete)

    except Exception as e:
        print(f"[-] Error in queue worker: {e}")
        import traceback
        traceback.print_exc()

@bot.event
async def on_member_remove(member):
    if member.guild.id != int(TARGET_GUILD_ID):
        return
        
    discord_id = str(member.id)
    
    try:
        all_user_data = await asyncio.to_thread(db.reference("user_data").get)
        if not all_user_data:
            return

        target_gd_id = None
        target_uid = None
        for uid, u_val in all_user_data.items():
            if u_val and str(u_val.get("discord_id")) == discord_id:
                target_uid = uid
                target_gd_id = str(u_val.get("account_id"))
                break

        if not target_gd_id or not target_uid:
            return

        await asyncio.to_thread(db.reference(f"users/{target_gd_id}").delete)
        await asyncio.to_thread(db.reference(f"user_data/{target_uid}").delete)
        print(f"[+] Successfully cleaned up data for Discord user {discord_id} (GD: {target_gd_id})")
    except Exception as e:
        print(f"[-] Failed to delete data for member {discord_id}: {e}")

@tasks.loop(seconds=5)
async def process_unlink_queue():
    try:
        all_user_data = await asyncio.to_thread(db.reference("user_data").get)
        if not all_user_data:
            return

        for uid, u_val in all_user_data.items():
            if u_val and u_val.get("unlink_requested") is True:
                discord_id = u_val.get("discord_id")
                account_id = u_val.get("account_id")

                if discord_id and account_id:
                    print(f"[*] Processing unlink request for GD: {account_id}, Discord: {discord_id}")
                    
                    try:
                        guild = bot.get_guild(int(TARGET_GUILD_ID))
                        if guild:
                            member = guild.get_member(int(discord_id))
                            if member:
                                await member.kick(reason="User unlinked their Geometry Dash account")
                    except Exception as e:
                        print(f"[-] Failed to kick user {discord_id}: {e}")

                    await asyncio.to_thread(db.reference(f"users/{account_id}").delete)
                    await asyncio.to_thread(db.reference(f"user_data/{uid}").delete)
                    print(f"[+] Successfully deleted data for {account_id}")

    except Exception as e:
        print(f"[-] Error in process_unlink_queue: {e}")

@bot.event
async def on_presence_update(before, after):
    discord_id = str(after.id)
    
    all_user_data = await asyncio.to_thread(db.reference("user_data").get)
    if not all_user_data:
        return
        
    gd_account_id = None
    user_settings = {}
    for uid, u_val in all_user_data.items():
        if u_val and str(u_val.get("discord_id")) == discord_id:
            gd_account_id = str(u_val.get("account_id"))
            user_settings = u_val.get("settings", {})
            break
            
    if not gd_account_id:
        return

    is_active = await asyncio.to_thread(db.reference(f"users/{gd_account_id}/is_active").get)
    if not is_active:
        return

    s_playing = user_settings.get("show_playing", True)
    s_streaming = user_settings.get("show_streaming", True)
    s_listening = user_settings.get("show_listening", True)
    s_watching = user_settings.get("show_watching", True)
    s_competing = user_settings.get("show_competing", True)

    rpc_list = []
    
    for activity in after.activities:
        # Ignore Custom Statuses
        if isinstance(activity, discord.CustomActivity):
            continue

        act_type = getattr(activity.type, 'value', 0)

        if act_type == 0 and not s_playing:
            continue
        if act_type == 1 and not s_streaming:
            continue
        if act_type == 2 and not s_listening:
            continue
        if act_type == 3 and not s_watching:
            continue
        if act_type == 5 and not s_competing:
            continue

        act_data = {
            "name": activity.name,
            "type": act_type,
            "details": getattr(activity, 'details', None),
            "state": getattr(activity, 'state', None),
            "assets": {
                "large_image": None,
                "large_text": None,
                "small_image": None,
                "small_text": None
            },
            "timestamps": {
                "start": None,
                "end": None
            },
            "color": None,
            "duration": None
        }

        # Timestamps
        if hasattr(activity, 'start') and activity.start:
            act_data["timestamps"]["start"] = int(activity.start.timestamp())
        elif hasattr(activity, 'created_at') and activity.created_at:
            act_data["timestamps"]["start"] = int(activity.created_at.timestamp())

        if hasattr(activity, 'end') and activity.end:
            act_data["timestamps"]["end"] = int(activity.end.timestamp())

        # Spotify :3
        if isinstance(activity, discord.Spotify):
            act_data["name"] = "Spotify"
            act_data["details"] = activity.title
            act_data["state"] = ', '.join(activity.artists)
            act_data["assets"]["large_image"] = activity.album_cover_url
            act_data["assets"]["large_text"] = activity.album
            act_data["color"] = str(activity.color) if activity.color else None # So it turns out this only returns a single color... but I'm not changing it because I already added the thing on the client so uhh yeah :3
            
            if hasattr(activity, 'duration') and activity.duration:
                act_data["duration"] = int(activity.duration.total_seconds())

        # 5. Standard Rich Presence
        else:
            if hasattr(activity, 'large_image_url') and activity.large_image_url:
                act_data["assets"]["large_image"] = activity.large_image_url
            if hasattr(activity, 'large_image_text') and activity.large_image_text:
                act_data["assets"]["large_text"] = activity.large_image_text
            if hasattr(activity, 'small_image_url') and activity.small_image_url:
                act_data["assets"]["small_image"] = activity.small_image_url
            if hasattr(activity, 'small_image_text') and activity.small_image_text:
                act_data["assets"]["small_text"] = activity.small_image_text

        rpc_list.append(act_data)

    # Push to Firebase :3
    try:
        await asyncio.to_thread(db.reference(f"users/{gd_account_id}/rpc").set, rpc_list)
    except Exception as e:
        print(f"[-] Failed to update RPC for {gd_account_id}: {e}")

bot.run(os.getenv("DISCORD_BOT_TOKEN"))