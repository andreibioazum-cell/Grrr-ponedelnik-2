# Gifts (server API groundwork)

Gift IDs and cup prices are defined in `functions/index.js`:

| ID | Cost |
| --- | ---: |
| `rose` | 25 |
| `cake` | 100 |
| `bear` | 250 |
| `rocket` | 500 |

The `giftAction` callable accepts `{ action: "send", gift, recipient }` or (admins only) `{ action: "grant", gift, recipient }`. It verifies the caller's Firebase Auth identity, checks the recipient profile, debits the sender's cup balance with a Realtime Database transaction, then increments the recipient's gift inventory. Admin grants do not debit cups.

Deploy with `firebase deploy --only functions:giftAction`. The signed-in Account screen now contains the gift shop, inventory counts, recipient field, and send action. The in-game CCP panel has a Gifts tab for admin grants. The database rules allow the gift inventory schema but do not let clients write gifts; only the Admin SDK callable can do so.
