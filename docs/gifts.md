# In-game gifts

Gifts work through the Realtime Database directly — there is no Cloud Function
(`giftAction`) anymore. Gift IDs and cup prices live in the game client
(`src/engine/network/gifts.inc`; the UI reads them via `net_gift_price`):

| ID | Cost |
| --- | ---: |
| `rose` | 25 |
| `cake` | 100 |
| `bear` | 250 |
| `rocket` | 500 |

## Data layout

```
users/<nick>/cups          cup balance
users/<nick>/gifts/rose    0..999 owned stickers (cake, bear, rocket likewise)
```

* **Buy** — one authenticated PATCH to `users/<nick>.json` that writes the new
  cup balance and the incremented sticker together, so a purchase cannot take
  the cups without delivering the sticker.
* **Send** — `-1` to the sender's `gifts` first, then `+1` to the recipient's.
  If the second write fails, the client puts the sticker back into the sender's
  counter.
* **Admin grant** — the CCP panel's Gifts tab performs the same `+1` write; it
  does not debit cups.

All requests are normal Firebase REST calls with the signed-in player's ID
token (`fb_http`), exactly like progress saves. No callable function is needed.

## Database rules

`firebase.rules.json` allows a signed-in `@cb4.game` password player to read
profiles, as before. For gifts specifically:

* the owner may set their own counters to any value `0..999`;
* another player may only write the same value or `+1` (one sticker per write);
* a write must contain all four counters (`hasChildren`), which also blocks
  deleting somebody else's gifts or individual counters;
* the rest of the profile (cups, levels, classes) stays owner-only, so gifting
  cannot change anything besides the gift counter.

Deploy the rules with:

```
firebase deploy --only database
```

If an older build had the `giftAction` callable deployed, remove it (nothing in
the game calls it anymore):

```
firebase functions:delete giftAction
```

The cup balance is client-written, same trust model as the rest of the
progress save, so a modified client could still fake its own cups — but not
another player's profile.

## UI

The signed-in Account screen contains a scrollable sticker shop, inventory
counts, a buy-for-self action, a recipient field, and a send-owned-gift action.
The in-game CCP panel has a Gifts tab for admin grants.
