# GeoRescuX — Enterprise Admin Provisioning Utility

This sub-package contains trusted server-side provisioning tools for GeoRescuX. In GeoRescuX, Administrator privileges are cryptographically enforced via **Firebase Auth Custom Claims** (`auth.token.admin == true`).

Because custom claims provide zero-trust security enforced by the Firebase Auth server and checked by `database.rules.json`, they **cannot** be forged or assigned from the Android client application.

---

## 📁 Directory Contents

| Script | Purpose |
| :--- | :--- |
| `makeAdmin.js` | Direct script to mint an administrator custom claim for a specific user UID |
| `manageAdmin.js` | Interactive command-line utility to list, inspect, grant, and revoke admin rights |
| `removeAdmin.js` | Direct script to revoke administrator custom claims and invalidate active refresh tokens |

---

## 🚀 Prerequisites & Setup

1. **Node.js** (v18+)
2. **Service Account Key**: Download `serviceAccountKey.json` from the Firebase Console (*Project Settings > Service Accounts > Generate new private key*).
3. Place `serviceAccountKey.json` inside this directory (this file is `.gitignore`d for security).

```bash
cd georescux-admin-provision
npm install
```

---

## 🛠️ Usage

### 1. Interactive Admin Manager
```bash
node manageAdmin.js
```
Interactive menu allows:
- Listing all users with admin privileges
- Looking up a user by email or UID
- Elevating a user to Admin
- Revoking Admin rights

### 2. Direct Granting
Edit `makeAdmin.js` with the target user's UID, then run:
```bash
node makeAdmin.js
```

### 3. Direct Revocation
Edit `removeAdmin.js` with the target user's UID, then run:
```bash
node removeAdmin.js
```

> **Note**: After claims are updated, the user must log out and log back in on the Android device for the new JWT token with custom claims to take effect.
