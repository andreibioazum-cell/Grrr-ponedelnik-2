"use strict";

/* adminResetPassword - lets the two game admins reset ANOTHER player's
 * account password from inside the game (chat command:
 *   passwd <nick> <new password>
 * ).
 *
 * A game client can never do this by itself: Firebase Auth changes a
 * password only with that account's own ID token. This callable Cloud
 * Function is the server side - it verifies the caller's ID token,
 * checks the caller is one of the admin accounts, and only then uses
 * the Admin SDK to set the password.
 *
 * The password stored in Firebase Auth is "<player password>" +
 * CB4_PASS_SUFFIX (the same secret tail the game client appends, see
 * src/engine/network/write_secret.inc). The suffix lives ONLY in the
 * function secret, never in this file:
 *   firebase functions:secrets:set CB4_PASS_SUFFIX
 * (print the current value with: python3 tools/rotate_db_secret.py --show)
 *
 * Deploy:
 *   cd functions && npm install
 *   firebase deploy --only functions:adminResetPassword
 */

const { onCall, HttpsError } = require("firebase-functions/v2/https");
const { defineSecret } = require("firebase-functions/params");
const admin = require("firebase-admin");

admin.initializeApp();

const passSuffix = defineSecret("CB4_PASS_SUFFIX");

const ADMIN_EMAILS = ["dimasi4ek229@cb4.game", "qwertyuiopaj1234@cb4.game"];
const EMAIL_DOMAIN = "@cb4.game";
const NICK_RE = /^[A-Za-z0-9_]{1,16}$/;

function bad(code, message) {
  return new HttpsError(code, message);
}

// Gift catalogue and server-authoritative gifting. The sender's cup balance is
// changed with an RTDB transaction (never trusted from the client); gift counts
// are stored on the recipient profile. CCP grants use the same endpoint and
// are restricted to the two existing admin accounts.
const GIFT_CATALOG = Object.freeze({ rose: 25, cake: 100, bear: 250, rocket: 500 });
const giftDb = admin.database();
function giftCaller(req) {
  if (!req.auth || !req.auth.token || !req.auth.token.email) {
    throw bad("unauthenticated", "sign-in required");
  }
  const email = String(req.auth.token.email).toLowerCase();
  if (!email.endsWith(EMAIL_DOMAIN)) throw bad("permission-denied", "game account required");
  return { email, nick: email.slice(0, -EMAIL_DOMAIN.length), admin: ADMIN_EMAILS.includes(email) };
}
exports.giftAction = onCall({ region: "us-central1" }, async (req) => {
  const caller = giftCaller(req);
  const data = req.data || {};
  const id = typeof data.gift === "string" ? data.gift : "";
  const cost = GIFT_CATALOG[id];
  const action = data.action;
  const recipient = typeof data.recipient === "string" ? data.recipient.trim() : "";
  const senderNick = typeof data.sender === "string" ? data.sender.trim() : caller.nick;
  if (!NICK_RE.test(senderNick) || senderNick.toLowerCase() !== caller.nick.toLowerCase()) {
    throw bad("permission-denied", "sender identity mismatch");
  }
  if (!cost) throw bad("invalid-argument", "unknown gift");
  if (action === "grant" && !caller.admin) throw bad("permission-denied", "admins only");
  if (!["buy", "send", "grant"].includes(action)) throw bad("invalid-argument", "bad action");
  const targetNick = action === "buy" ? senderNick : recipient;
  if (!NICK_RE.test(targetNick)) throw bad("invalid-argument", "bad recipient");
  if (action === "send" && targetNick.toLowerCase() === caller.nick.toLowerCase()) {
    throw bad("failed-precondition", "cannot gift yourself");
  }
  const target = giftDb.ref(`users/${targetNick}`);
  if (!(await target.get()).exists()) throw bad("not-found", "recipient not found");
  let senderChanged = false;
  let inventoryError = "";
  if (action !== "grant") {
    const sender = giftDb.ref(`users/${senderNick}`);
    const debit = await sender.transaction((profile) => {
      if (!profile || typeof profile !== "object") return;
      profile.gifts = profile.gifts && typeof profile.gifts === "object" ? profile.gifts : {};
      const owned = Number(profile.gifts[id] || 0);
      if (action === "send") {
        if (owned < 1) { inventoryError = "no gift in inventory"; return; }
        profile.gifts[id] = owned - 1;
      } else {
        const cups = Number(profile.cups || 0);
        if (cups < cost) { inventoryError = "not enough cups"; return; }
        profile.cups = cups - cost;
        profile.gifts[id] = Math.min(999, owned + 1);
      }
      return profile;
    });
    if (!debit.committed) {
      if (inventoryError === "not enough cups") throw bad("failed-precondition", inventoryError);
      if (inventoryError) throw bad("failed-precondition", inventoryError);
      throw bad("failed-precondition", "sender profile unavailable");
    }
    senderChanged = true;
    if (action === "buy") return { ok: true, gift: id, owned: Number(debit.snapshot.child(`gifts/${id}`).val() || 0) };
  }
  try {
    const result = await target.transaction((profile) => {
      if (!profile || typeof profile !== "object") return;
      profile.gifts = profile.gifts && typeof profile.gifts === "object" ? profile.gifts : {};
      profile.gifts[id] = Math.min(999, Number(profile.gifts[id] || 0) + 1);
      return profile;
    });
    if (!result.committed) throw bad("not-found", "recipient not found");
  } catch (e) {
    if (senderChanged) {
      await giftDb.ref(`users/${senderNick}`).transaction((profile) => {
        if (!profile || !profile.gifts) return;
        if (action === "send") profile.gifts[id] = Number(profile.gifts[id] || 0) + 1;
        return profile;
      });
    }
    throw e;
  }
  return { ok: true, gift: id };
});

exports.adminResetPassword = onCall(
  { region: "us-central1", secrets: [passSuffix] },
  async (req) => {
    if (!req.auth || !req.auth.token || !req.auth.token.email) {
      throw bad("unauthenticated", "sign-in required");
    }
    const caller = String(req.auth.token.email).toLowerCase();
    if (!ADMIN_EMAILS.includes(caller)) {
      throw bad("permission-denied", "admins only");
    }

    const data = req.data || {};
    const nick = typeof data.nick === "string" ? data.nick.trim() : "";
    const password = typeof data.password === "string" ? data.password : "";
    if (!NICK_RE.test(nick)) {
      throw bad("invalid-argument", "bad nick");
    }
    if (
      password.length < 1 ||
      password.length > 32 ||
      /[\s"\\]/.test(password) ||
      /[\u0000-\u001F\u007F]/.test(password)
    ) {
      throw bad("invalid-argument", "bad password");
    }

    const email = nick.toLowerCase() + EMAIL_DOMAIN;
    if (ADMIN_EMAILS.includes(email)) {
      /* Admin accounts are protected, same as the in-game ban rule. */
      throw bad("permission-denied", "protected account");
    }

    let user;
    try {
      user = await admin.auth().getUserByEmail(email);
    } catch (e) {
      if (e && e.code === "auth/user-not-found") {
        throw bad("not-found", "no such account");
      }
      throw bad("internal", "lookup failed");
    }

    await admin.auth().updateUser(user.uid, {
      password: password + passSuffix.value(),
    });
    /* Kick the player's old sessions: the old password stops working
     * everywhere, not just on paper. */
    await admin.auth().revokeRefreshTokens(user.uid);

    return { ok: true };
  }
);
