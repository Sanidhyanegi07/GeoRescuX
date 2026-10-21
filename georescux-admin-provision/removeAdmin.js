const { initializeApp, cert } = require('firebase-admin/app');
const { getAuth } = require('firebase-admin/auth');

const serviceAccount = require('./serviceAccountKey.json');
initializeApp({ credential: cert(serviceAccount) });

const targetUid = process.argv[2];
if (!targetUid) {
  console.error('Usage: node removeAdmin.js <firebase-user-uid>');
  process.exit(2);
}

getAuth().getUser(targetUid)
  .then((user) => getAuth().setCustomUserClaims(targetUid, {
    ...(user.customClaims || {}),
    admin: false
  }))
  .then(() => getAuth().revokeRefreshTokens(targetUid))
  .then(() => {
    console.log(`Admin claim revoked and refresh tokens invalidated for UID: ${targetUid}`);
    console.log('The user must sign in again to receive a fresh token.');
  })
  .catch((error) => {
    console.error('Error removing custom claims:', error.message);
    process.exitCode = 1;
  });
