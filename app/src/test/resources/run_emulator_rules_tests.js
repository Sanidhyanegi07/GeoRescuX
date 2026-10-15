const http = require('http');

const NS = 'demo-georescux-default-rtdb';

const aliceAuth = { uid: "alice_uid", token: { admin: false } };
const bobAuth = { uid: "bob_uid", token: { admin: false } };
const adminAuth = { uid: "admin_uid", token: { admin: true } };

function req(path, method, body, authOverride) {
  return new Promise((resolve, reject) => {
    let query = `?ns=${NS}&access_token=owner`;
    if (authOverride !== undefined) {
      query += `&auth_variable_override=${encodeURIComponent(JSON.stringify(authOverride))}`;
    }
    const r = http.request({
      hostname: '127.0.0.1',
      port: 9000,
      path: path + query,
      method: method,
      headers: { 'Content-Type': 'application/json' }
    }, res => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => resolve({ status: res.statusCode, body: data }));
    });
    r.on('error', reject);
    if (body !== undefined && body !== null) r.write(JSON.stringify(body));
    r.end();
  });
}

async function runTests() {
  console.log("\n========================================================");
  console.log("GENUINE FIREBASE REALTIME DATABASE EMULATOR RULES TESTS");
  console.log("========================================================\n");

  let passed = 0;
  let failed = 0;

  async function assertCase(name, expectedSuccess, promise) {
    const res = await promise;
    const isSuccess = res.status >= 200 && res.status < 300 && !res.body.includes("Permission denied");
    const ok = isSuccess === expectedSuccess;
    if (ok) {
      console.log(`[PASS] ${name} (Status: ${res.status}${!expectedSuccess ? ' - Correctly Denied' : ' - Allowed'})`);
      passed++;
    } else {
      console.error(`[FAIL] ${name} (Expected success: ${expectedSuccess}, got status: ${res.status}, body: ${res.body.trim()})`);
      failed++;
    }
  }

  // 1. Unauthenticated read of SOS alerts root
  await assertCase(
    "1. Unauthenticated user cannot read /sos_alerts",
    false,
    req('/sos_alerts.json', 'GET', null, null)
  );

  // 2. Unrelated user (Bob) cannot read Alice's SOS alerts
  await assertCase(
    "2. Unrelated user (Bob) cannot read Alice's SOS alerts",
    false,
    req('/sos_alerts/alice_uid.json', 'GET', null, bobAuth)
  );

  // 3. Unrelated user (Bob) cannot write to Alice's SOS alerts
  await assertCase(
    "3. Unrelated user (Bob) cannot write to Alice's SOS alert",
    false,
    req('/sos_alerts/alice_uid/alert_101.json', 'PUT', { id: "alert_101", status: "ACTIVE", startedAtMs: 1729000000000 }, bobAuth)
  );

  // 4. Owner (Alice) creates SOS with ACTIVE
  const activePayload = {
    id: "alert_101",
    status: "ACTIVE",
    startedAtMs: 1729000000000,
    stoppedAtMs: null,
    location: {
      latitude: 30.55,
      longitude: 79.56,
      accuracyMeters: 8.5,
      timestampMs: 1729000000000,
      provider: "GPS"
    },
    locationStatus: "ACQUIRED",
    note: "Injured hiker near ridge"
  };
  await assertCase(
    "4. Owner (Alice) creates and updates an SOS with ACTIVE",
    true,
    req('/sos_alerts/alice_uid/alert_101.json', 'PUT', activePayload, aliceAuth)
  );

  // 5. Owner (Alice) completes SOS with COMPLETED
  const completedPayload = {
    ...activePayload,
    status: "COMPLETED",
    stoppedAtMs: 1729000100000
  };
  await assertCase(
    "5. Owner (Alice) completes an SOS with COMPLETED",
    true,
    req('/sos_alerts/alice_uid/alert_101.json', 'PUT', completedPayload, aliceAuth)
  );

  // 6. Owner (Alice) cannot set status to RESOLVED
  await assertCase(
    "6. Owner (Alice) cannot set status to RESOLVED directly",
    false,
    req('/sos_alerts/alice_uid/alert_101/status.json', 'PUT', "RESOLVED", aliceAuth)
  );

  // 7. Owner (Alice) cannot set isVerified on self
  await assertCase(
    "7. Owner (Alice) cannot set isVerified=true on self",
    false,
    req('/sos_alerts/alice_uid/isVerified.json', 'PUT', true, aliceAuth)
  );

  // 8. Owner (Alice) cannot delete complete alert record
  await assertCase(
    "8. Owner (Alice) cannot delete alert record (DELETE)",
    false,
    req('/sos_alerts/alice_uid/alert_101.json', 'DELETE', null, aliceAuth)
  );

  // 9. Admin can set isVerified to boolean true
  await assertCase(
    "9. Admin can set isVerified to boolean true on Alice",
    true,
    req('/sos_alerts/alice_uid/isVerified.json', 'PUT', true, adminAuth)
  );

  // 10. Admin cannot delete isVerified
  await assertCase(
    "10. Admin cannot delete isVerified (DELETE)",
    false,
    req('/sos_alerts/alice_uid/isVerified.json', 'DELETE', null, adminAuth)
  );

  // 11. Admin can change another user's SOS status to RESOLVED
  await assertCase(
    "11. Admin can change Alice's SOS status to RESOLVED",
    true,
    req('/sos_alerts/alice_uid/alert_101/status.json', 'PUT', "RESOLVED", adminAuth)
  );

  // 12. Admin cannot reopen alert by setting status to ACTIVE
  await assertCase(
    "12. Admin cannot reopen alert by setting status to ACTIVE",
    false,
    req('/sos_alerts/alice_uid/alert_101/status.json', 'PUT', "ACTIVE", adminAuth)
  );

  // 13. Admin cannot delete status field
  await assertCase(
    "13. Admin cannot delete status field (DELETE)",
    false,
    req('/sos_alerts/alice_uid/alert_101/status.json', 'DELETE', null, adminAuth)
  );

  // 14. Admin cannot alter Alice's GPS location telemetry
  await assertCase(
    "14. Admin cannot modify Alice's GPS location telemetry",
    false,
    req('/sos_alerts/alice_uid/alert_101/location.json', 'PUT', { latitude: 20.0, longitude: 70.0 }, adminAuth)
  );

  // 15. Admin cannot alter Alice's situation note
  await assertCase(
    "15. Admin cannot modify Alice's situation note",
    false,
    req('/sos_alerts/alice_uid/alert_101/note.json', 'PUT', "Tampered note", adminAuth)
  );

  // 16. Admin cannot alter Alice's startedAtMs timestamp
  await assertCase(
    "16. Admin cannot modify Alice's startedAtMs timestamp",
    false,
    req('/sos_alerts/alice_uid/alert_101/startedAtMs.json', 'PUT', 111111111, adminAuth)
  );

  // 17. SOS history upsert using actual SosCloudPayload.fromHistory output (simulating PATCH/updateChildren)
  const historyMap = {
    "alert_101": completedPayload,
    "alert_102": {
      id: "alert_102",
      status: "ACTIVE",
      startedAtMs: 1729000200000,
      stoppedAtMs: null,
      location: null,
      locationStatus: null,
      note: null
    }
  };
  await assertCase(
    "17. SOS history upsert using updateChildren (PATCH) succeeds without touching isVerified",
    true,
    req('/sos_alerts/alice_uid.json', 'PATCH', historyMap, aliceAuth)
  );

  // 18. Verify isVerified is still intact after history upsert
  const verifyCheck = await req('/sos_alerts/alice_uid/isVerified.json', 'GET', null, adminAuth);
  const verifyIntact = verifyCheck.status === 200 && verifyCheck.body.trim() === 'true';
  if (verifyIntact) {
    console.log("[PASS] 18. isVerified remained intact (true) after history upsert");
    passed++;
  } else {
    console.error("[FAIL] 18. isVerified was overwritten: " + verifyCheck.body);
    failed++;
  }

  // 19. /events access restriction
  await assertCase(
    "19. Ordinary user (Alice) cannot read /events",
    false,
    req('/events.json', 'GET', null, aliceAuth)
  );
  await assertCase(
    "20. Admin can read /events",
    true,
    req('/events.json', 'GET', null, adminAuth)
  );

  // 21. /incidents permissions
  await assertCase(
    "21. Ordinary user (Alice) cannot read /incidents",
    false,
    req('/incidents.json', 'GET', null, aliceAuth)
  );
  await assertCase(
    "22. Admin can read /incidents",
    true,
    req('/incidents.json', 'GET', null, adminAuth)
  );
  await assertCase(
    "23. Admin can create an incident in /incidents",
    true,
    req('/incidents/inc_999.json', 'PUT', { type: "FLOOD", description: "River cresting", status: "REPORTED", timestampMs: 1729000300000 }, adminAuth)
  );
  await assertCase(
    "24. Ordinary user (Alice) cannot create an incident in /incidents",
    false,
    req('/incidents/inc_999_fake.json', 'PUT', { type: "FLOOD", description: "Fake", status: "REPORTED", timestampMs: 1729000300000 }, aliceAuth)
  );

  // 22. /verification_requests read restrictions
  await assertCase(
    "25. Unauthenticated user cannot read /verification_requests",
    false,
    req('/verification_requests.json', 'GET', null, null)
  );
  await assertCase(
    "26. Unauthenticated user cannot read Alice's verification request",
    false,
    req('/verification_requests/alice_uid.json', 'GET', null, null)
  );
  await assertCase(
    "27. Unrelated user (Bob) cannot read Alice's verification request",
    false,
    req('/verification_requests/alice_uid.json', 'GET', null, bobAuth)
  );
  await assertCase(
    "28. Admin can read all verification requests",
    true,
    req('/verification_requests.json', 'GET', null, adminAuth)
  );

  // 23. /verification_requests write lifecycle & least privilege
  const aliceValidRequest = {
    uid: "alice_uid",
    name: "Alice Walker",
    timestampMs: 1729000000000,
    status: "PENDING"
  };

  await assertCase(
    "29. User (Alice) submits legitimate verification request with PENDING",
    true,
    req('/verification_requests/alice_uid.json', 'PUT', aliceValidRequest, aliceAuth)
  );

  await assertCase(
    "30. User (Alice) can read her own verification request",
    true,
    req('/verification_requests/alice_uid.json', 'GET', null, aliceAuth)
  );

  await assertCase(
    "31. User (Alice) cannot submit request with VERIFIED status directly",
    false,
    req('/verification_requests/alice_uid.json', 'PUT', { ...aliceValidRequest, status: "VERIFIED" }, aliceAuth)
  );

  await assertCase(
    "32. User (Alice) cannot write directly to status child to set VERIFIED",
    false,
    req('/verification_requests/alice_uid/status.json', 'PUT', "VERIFIED", aliceAuth)
  );

  await assertCase(
    "33. User (Alice) cannot delete verification request (DELETE)",
    false,
    req('/verification_requests/alice_uid.json', 'DELETE', null, aliceAuth)
  );

  await assertCase(
    "34. Unrelated user (Bob) cannot write Alice's verification request",
    false,
    req('/verification_requests/alice_uid.json', 'PUT', { ...aliceValidRequest, name: "Bob Tamper" }, bobAuth)
  );

  await assertCase(
    "35. Admin approves Alice's verification request (status: VERIFIED)",
    true,
    req('/verification_requests/alice_uid/status.json', 'PUT', "VERIFIED", adminAuth)
  );

  await assertCase(
    "36. User (Alice) cannot overwrite or demote status back to PENDING once approved",
    false,
    req('/verification_requests/alice_uid.json', 'PUT', { ...aliceValidRequest, timestampMs: 1729000400000 }, aliceAuth)
  );

  const bobValidRequest = {
    uid: "bob_uid",
    name: "Bob Builder",
    timestampMs: 1729000000000,
    status: "PENDING"
  };
  await assertCase(
    "37. Bob submits legitimate verification request with PENDING",
    true,
    req('/verification_requests/bob_uid.json', 'PUT', bobValidRequest, bobAuth)
  );

  await assertCase(
    "38. Admin rejects Bob's verification request (status: REJECTED)",
    true,
    req('/verification_requests/bob_uid/status.json', 'PUT', "REJECTED", adminAuth)
  );

  await assertCase(
    "39. Admin cannot delete status from a verification request (DELETE)",
    false,
    req('/verification_requests/bob_uid/status.json', 'DELETE', null, adminAuth)
  );

  // 40. Owner (Alice) writes user profile under /users/alice_uid
  await assertCase(
    "40. Owner (Alice) writes user profile under /users/alice_uid",
    true,
    req('/users/alice_uid.json', 'PUT', { email: "alice@example.com", name: "Alice", regionId: "uttarakhand" }, aliceAuth)
  );

  // 41. Unrelated user (Bob) cannot read Alice's profile under /users/alice_uid
  await assertCase(
    "41. Unrelated user (Bob) cannot read Alice's profile under /users/alice_uid",
    false,
    req('/users/alice_uid.json', 'GET', null, bobAuth)
  );

  // 42. Unrelated user (Bob) cannot write to Alice's profile under /users/alice_uid
  await assertCase(
    "42. Unrelated user (Bob) cannot write to Alice's profile under /users/alice_uid",
    false,
    req('/users/alice_uid.json', 'PUT', { email: "hacked@example.com" }, bobAuth)
  );

  // 43. Admin can read all users under /users
  await assertCase(
    "43. Admin can read all users under /users",
    true,
    req('/users.json', 'GET', null, adminAuth)
  );

  console.log(`\n========================================================`);
  console.log(`REAL FIREBASE EMULATOR RESULTS: ${passed} PASSED, ${failed} FAILED out of ${passed + failed} tests`);
  console.log(`========================================================\n`);

  if (failed > 0) process.exit(1);
}

runTests().catch(err => {
  console.error("Test execution failed:", err);
  process.exit(1);
});
