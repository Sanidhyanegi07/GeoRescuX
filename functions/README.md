# GeoRescuX — Firebase Cloud Functions Subsystem

Serverless event handlers and secure callable functions for the GeoRescuX disaster emergency platform.

---

## 🏛️ Architecture & Capabilities

1. **`setAdminClaim` (Callable Function)**:
   - Grants or revokes the `{ admin: true }` custom claim on target user UIDs.
   - Enforces zero-trust privilege checks: caller must already be a verified administrator (`context.auth.token.admin === true`).
   - Merges existing claims to prevent destructive overwrites of unrelated user metadata.
   - Automatically revokes refresh tokens on downgrade to force immediate security credential invalidation.

2. **Role Verification**:
   - Cryptographically binds administrative role validation to the Firebase Authentication server infrastructure, preventing client-side spoofing.

---

## 🧪 Testing

The unit test suite runs against the Firebase Functions test SDK:
```bash
cd functions
npm test
```
