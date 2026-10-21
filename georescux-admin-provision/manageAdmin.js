/**
 * GeoRescuX Admin Management CLI
 * 
 * Usage:
 *   node manageAdmin.js grant <uid>   - Grants admin custom claim (admin: true)
 *   node manageAdmin.js revoke <uid>  - Revokes admin custom claim (admin: false)
 *   node manageAdmin.js verify <uid>  - Inspects user's custom claims
 *
 * Default target UID if omitted: oJDDWyVlmNPMyfNSFQqptcActiF2
 */

const { initializeApp, cert } = require('firebase-admin/app');
const { getAuth } = require('firebase-admin/auth');

const serviceAccount = require('./serviceAccountKey.json');

initializeApp({
  credential: cert(serviceAccount)
});

const command = process.argv[2]?.trim().toLowerCase();
const targetUid = process.argv[3]?.trim();

const VALID_COMMANDS = ['grant', 'revoke', 'verify'];
if (!command || !VALID_COMMANDS.includes(command)) {
  console.error("Usage: node manageAdmin.js <grant|revoke|verify> <target-user-uid>");
  console.error("  grant  <uid>  - Safely grants admin: true claim, merging existing claims");
  console.error("  revoke <uid>  - Revokes admin claim (admin: false), preserves other claims, and invalidates refresh tokens");
  console.error("  verify <uid>  - Inspects target user profile, existence, and current claims");
  process.exit(2);
}

if (!targetUid || targetUid.length === 0) {
  console.error(`Error: Missing required <target-user-uid> argument for command '${command}'.`);
  console.error("Usage: node manageAdmin.js <grant|revoke|verify> <target-user-uid>");
  process.exit(2);
}

async function run() {
  const auth = getAuth();

  // Verify target user existence before performing claims operations
  const user = await auth.getUser(targetUid);
  const existingClaims = user.customClaims || {};

  switch (command) {
    case 'grant': {
      const mergedClaims = { ...existingClaims, admin: true };
      await auth.setCustomUserClaims(targetUid, mergedClaims);
      console.log(`[GeoRescuX Admin] Successfully granted admin: true claim to UID: ${targetUid}`);
      console.log(`[GeoRescuX Admin] Preserved existing claims:`, existingClaims);
      console.log(`[GeoRescuX Admin] Updated claims:`, mergedClaims);
      break;
    }

    case 'revoke': {
      const mergedClaims = { ...existingClaims, admin: false };
      await auth.setCustomUserClaims(targetUid, mergedClaims);
      await auth.revokeRefreshTokens(targetUid);
      console.log(`[GeoRescuX Admin] Successfully revoked admin claim (admin: false) for UID: ${targetUid}`);
      console.log(`[GeoRescuX Admin] Preserved existing claims:`, existingClaims);
      console.log(`[GeoRescuX Admin] Invalidated active refresh tokens for UID: ${targetUid}`);
      break;
    }

    case 'verify': {
      console.log(`[GeoRescuX Admin] User UID: ${user.uid}`);
      console.log(`[GeoRescuX Admin] Email: ${user.email || 'N/A'}`);
      console.log(`[GeoRescuX Admin] Custom Claims:`, user.customClaims || {});
      const isAdmin = user.customClaims && user.customClaims.admin === true;
      console.log(`[GeoRescuX Admin] Admin Status: ${isAdmin ? 'ACTIVE ADMIN (admin: true)' : 'NORMAL USER'}`);
      break;
    }
  }
}

run()
  .then(() => process.exit(0))
  .catch((err) => {
    if (err.code === 'auth/user-not-found') {
      console.error(`[GeoRescuX Admin] Error: User with UID '${targetUid}' not found in Firebase project.`);
    } else {
      console.error(`[GeoRescuX Admin] Error executing command '${command}' for UID '${targetUid}':`, err.message || err);
    }
    process.exit(1);
  });
