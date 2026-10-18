# In-game gifts

Gift IDs and cup prices are defined in `functions/index.js`:

| ID | Cost |
| --- | ---: |
| `rose` | 25 |
| `cake` | 100 |
| `bear` | 250 |
| `rocket` | 500 |

The `giftAction` callable supports `{ action: "buy", gift, sender }` to purchase for the signed-in player's own collection, `{ action: "send", gift, sender, recipient }` to transfer one owned gift, and (admins only) `{ action: "grant", gift, sender, recipient }`. It verifies the caller's Firebase Auth identity and uses Realtime Database transactions for cup debits and inventory changes. Buying charges cups once; sending an owned sticker does not charge again. Admin grants do not debit cups.

Deploy with `firebase deploy --only functions:giftAction`. The signed-in Account screen contains a scrollable sticker shop, inventory counts, a buy-for-self action, recipient field, and send-owned-gift action. Buying debits cups once and adds the sticker to the player's collection; gifting transfers one owned sticker without charging cups again. The in-game CCP panel has a Gifts tab for admin grants. The database rules allow the gift inventory schema but do not let clients write gifts; only the Admin SDK callable can do so.
