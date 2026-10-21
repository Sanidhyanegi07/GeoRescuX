/**
 * Security and Authorization Unit Tests for GeoRescuX Firebase Cloud Functions
 * 
 * Verifies remediation of FINDING-SEC-01 (Privilege Escalation via Self-Bootstrap)
 * and confirms authorization boundaries for:
 * 1. Anonymous / unauthenticated callers
 * 2. Ordinary authenticated users attempting self-promotion or peer-promotion
 * 3. Legitimate administrator management workflows (grant and revoke)
 * 4. Status inspection permission boundaries
 */

const test = require("node:test");
const assert = require("node:assert/strict");
const Module = require("module");

// In-memory mock database of users and claims
const userStore = {
  "user_ordinary": { uid: "user_ordinary", customClaims: { volunteer: true } },
  "user_attacker": { uid: "user_attacker", customClaims: {} },
  "user_admin": { uid: "user_admin", customClaims: { admin: true, rank: "chief" } },
  "user_target": { uid: "user_target", customClaims: { department: "SAR" } }
};

// Mock Firebase Admin SDK and Firebase Functions without external dependencies
const origLoad = Module._load;
Module._load = function (request, parent, isMain) {
  if (request === "firebase-functions") {
    return {
      https: {
        onCall: (handler) => handler,
        HttpsError: class HttpsError extends Error {
          constructor(code, message) {
            super(message);
            this.name = "HttpsError";
            this.code = code;
          }
        }
      }
    };
  }
  if (request === "firebase-admin") {
    return {
      initializeApp: () => {},
      auth: () => ({
        getUser: async (uid) => {
          if (!userStore[uid]) {
            throw new Error(`User ${uid} not found`);
          }
          return JSON.parse(JSON.stringify(userStore[uid]));
        },
        setCustomUserClaims: async (uid, claims) => {
          if (!userStore[uid]) {
            throw new Error(`User ${uid} not found`);
          }
          userStore[uid].customClaims = { ...claims };
        },
        revokeRefreshTokens: async (uid) => {
          if (!userStore[uid]) {
            throw new Error(`User ${uid} not found`);
          }
          userStore[uid].tokensRevoked = true;
        }
      })
    };
  }
  return origLoad.apply(this, arguments);
};

// Load the Cloud Functions module under test
const { setAdminClaim, getAdminStatus } = require("../index.js");

test("Security Gate: setAdminClaim authorization enforcement", async (t) => {
  await t.test("Rejects anonymous/unauthenticated callers with 'unauthenticated'", async () => {
    await assert.rejects(
      async () => {
        await setAdminClaim({ targetUid: "user_target", admin: true }, { auth: null });
      },
      (err) => {
        assert.equal(err.code, "unauthenticated");
        assert.match(err.message, /must be signed in/i);
        return true;
      }
    );
  });

  await t.test("Rejects missing or invalid targetUid with 'invalid-argument'", async () => {
    const callerContext = { auth: { uid: "user_admin", token: { admin: true } } };

    await assert.rejects(
      async () => {
        await setAdminClaim({ targetUid: "", admin: true }, callerContext);
      },
      (err) => {
        assert.equal(err.code, "invalid-argument");
        return true;
      }
    );

    await assert.rejects(
      async () => {
        await setAdminClaim({ targetUid: 12345, admin: true }, callerContext);
      },
      (err) => {
        assert.equal(err.code, "invalid-argument");
        return true;
      }
    );
  });

  await t.test("CRITICAL REGRESSION (FINDING-SEC-01): Ordinary user attempting self-bootstrap MUST throw 'permission-denied'", async () => {
    // Attacker calls setAdminClaim with their own UID trying to gain admin privileges
    const attackerContext = {
      auth: {
        uid: "user_attacker",
        token: { admin: false }
      }
    };

    await assert.rejects(
      async () => {
        await setAdminClaim({ targetUid: "user_attacker", admin: true }, attackerContext);
      },
      (err) => {
        assert.equal(err.code, "permission-denied");
        assert.match(err.message, /Only verified administrators/i);
        return true;
      }
    );

    // Verify attacker did NOT acquire admin claims
    assert.equal(userStore["user_attacker"].customClaims.admin, undefined);
  });

  await t.test("Ordinary user attempting to elevate another user MUST throw 'permission-denied'", async () => {
    const callerContext = {
      auth: {
        uid: "user_ordinary",
        token: {}
      }
    };

    await assert.rejects(
      async () => {
        await setAdminClaim({ targetUid: "user_target", admin: true }, callerContext);
      },
      (err) => {
        assert.equal(err.code, "permission-denied");
        return true;
      }
    );

    assert.equal(userStore["user_target"].customClaims.admin, undefined);
  });

  await t.test("Legitimate administrator successfully grants admin claim and merges existing claims", async () => {
    const adminContext = {
      auth: {
        uid: "user_admin",
        token: { admin: true }
      }
    };

    const result = await setAdminClaim({ targetUid: "user_target", admin: true }, adminContext);
    assert.deepEqual(result, { status: "success", targetUid: "user_target", admin: true });
    assert.equal(userStore["user_target"].customClaims.admin, true);
    assert.equal(userStore["user_target"].customClaims.department, "SAR", "Existing claims must be preserved");
  });

  await t.test("Legitimate administrator successfully revokes admin claim", async () => {
    const adminContext = {
      auth: {
        uid: "user_admin",
        token: { admin: true }
      }
    };

    const result = await setAdminClaim({ targetUid: "user_target", admin: false }, adminContext);
    assert.deepEqual(result, { status: "success", targetUid: "user_target", admin: false });
    assert.equal(userStore["user_target"].customClaims.admin, false);
    assert.equal(userStore["user_target"].customClaims.department, "SAR");
    assert.equal(userStore["user_target"].tokensRevoked, true, "Revoking admin must invalidate refresh tokens to prevent cached session reuse");
  });

  await t.test("Fail-closed: Non-boolean truthy values (e.g. string 'true') do NOT grant admin privilege", async () => {
    const adminContext = {
      auth: {
        uid: "user_admin",
        token: { admin: true }
      }
    };

    const result = await setAdminClaim({ targetUid: "user_target", admin: "true" }, adminContext);
    assert.deepEqual(result, { status: "success", targetUid: "user_target", admin: false });
    assert.equal(userStore["user_target"].customClaims.admin, false, "admin claim must be false if not boolean true");
  });

  await t.test("Ordinary user attempting to revoke claims MUST throw 'permission-denied'", async () => {
    const callerContext = {
      auth: {
        uid: "user_ordinary",
        token: {}
      }
    };

    await assert.rejects(
      async () => {
        await setAdminClaim({ targetUid: "user_admin", admin: false }, callerContext);
      },
      (err) => {
        assert.equal(err.code, "permission-denied");
        return true;
      }
    );

    // Verify admin was not revoked by ordinary user
    assert.equal(userStore["user_admin"].customClaims.admin, true);
  });

  await t.test("Fail-closed: Internal backend errors throw 'internal'", async () => {
    const adminContext = {
      auth: {
        uid: "user_admin",
        token: { admin: true }
      }
    };

    await assert.rejects(
      async () => {
        await setAdminClaim({ targetUid: "nonexistent_uid", admin: true }, adminContext);
      },
      (err) => {
        assert.equal(err.code, "internal");
        assert.match(err.message, /Failed to update admin privileges/i);
        return true;
      }
    );
  });
});

test("Security Gate: getAdminStatus role inspection authorization", async (t) => {
  await t.test("Rejects anonymous caller with 'unauthenticated'", async () => {
    await assert.rejects(
      async () => {
        await getAdminStatus({}, { auth: null });
      },
      (err) => {
        assert.equal(err.code, "unauthenticated");
        return true;
      }
    );
  });

  await t.test("Allows ordinary user to inspect their own status", async () => {
    const context = {
      auth: {
        uid: "user_ordinary",
        token: {}
      }
    };

    const status = await getAdminStatus({}, context);
    assert.equal(status.uid, "user_ordinary");
    assert.equal(status.admin, false);
  });

  await t.test("Rejects ordinary user inspecting another user's status with 'permission-denied'", async () => {
    const context = {
      auth: {
        uid: "user_ordinary",
        token: {}
      }
    };

    await assert.rejects(
      async () => {
        await getAdminStatus({ targetUid: "user_admin" }, context);
      },
      (err) => {
        assert.equal(err.code, "permission-denied");
        return true;
      }
    );
  });

  await t.test("Allows verified administrator to inspect any user's status", async () => {
    const adminContext = {
      auth: {
        uid: "user_admin",
        token: { admin: true }
      }
    };

    const status = await getAdminStatus({ targetUid: "user_ordinary" }, adminContext);
    assert.equal(status.uid, "user_ordinary");
    assert.equal(status.admin, false);
  });
});
