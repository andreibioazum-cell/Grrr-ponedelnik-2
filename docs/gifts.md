# In-game gifts

Gifts work through the Realtime Database directly — there is no Cloud Function
(`giftAction`) involved. The catalogue lives in the client
(`src/engine/network/gifts.inc`; the UI reads the price via `net_gift_price`):

| ID | Sticker | Cost |
| --- | --- | ---: |
| `poop` | Какашка / Poop | 25 |
| `cake` | Торт / Cake | 100 |
| `bear` | Мишка / Teddy Bear | 250 |
| `rocket` | Ракета / Rocket | 500 |

The first sticker used to be `rose`; it is now `poop`. Old `rose` counters may
still sit in profiles — the rules keep accepting them, but the game neither
shows nor spends them.

## Data layout

```
users/<nick>/cups          cup balance
users/<nick>/gifts/poop    0..999 owned stickers (cake, bear, rocket likewise)
```

* **Buy** — one authenticated PATCH to `users/<nick>.json` that writes the new
  cup balance and the incremented sticker together, so a purchase cannot take
  the cups without delivering the sticker.
* **Send** — `-1` to the sender's `gifts/<id>` first, then `+1` to the
  recipient's. Each step is a single-child write, so the other counters are
  never rewritten. If the second write fails, the client puts the sticker back
  into the sender's counter.
* **Admin grant** — the CCP panel's Gifts tab performs the same `+1` write; it
  does not debit cups.

All requests are normal Firebase REST calls with the signed-in player's ID
token (`fb_http`), exactly like progress saves. No callable function is needed.

## Database rules

`firebase.rules.json` allows a signed-in `@cb4.game` password player to read
profiles, as before. For each gift counter:

* the owner may set it to any value `0..999`;
* another player may only move it from `N` to `N + 1` (one sticker per write),
  and may create a missing counter only with `1`;
* deletes are denied (`newData.exists()` in the `.write` rule), so nobody can
  wipe somebody else's collection;
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

* Every sticker is drawn from primitives (circles, rounded rectangles, lines,
  rotated rects) in `src/game/ui/gift_icons.inc`; the pixel font has no icons
  and is never asked for a symbol.
* The collection shows a small **heap** of owned stickers instead of a `× N`
  counter. Three icons at most, so any amount reads as "a pile".
* The list snaps to whole rows while scrolling: a half-visible row is never
  drawn above the rounded card, which used to make buttons show through the
  header.

The signed-in Account screen contains the shop, inventory heaps, a
buy-for-self action, a recipient field, and a send-owned-gift action. The
in-game CCP panel has a Gifts tab for admin grants.
