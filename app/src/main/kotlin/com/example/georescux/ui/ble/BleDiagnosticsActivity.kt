package com.example.georescux.ui.ble

import android.Manifest
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.example.georescux.GeoRescuXApplication
import com.example.georescux.R
import com.example.georescux.domain.ble.GeoRescueBleDiagnostics
import com.example.georescux.domain.ble.GeoRescueBlePacket
import com.example.georescux.domain.ble.GeoRescueBlePacketType

/**
 * Two-device BLE diagnostic screen (debug/diagnostic surface per the BLE
 * subsystem design). Shows the exact checklist required to classify a
 * two-device failure (discovery? connection? GATT? data?):
 *
 * - Bluetooth supported/enabled, BLE supported
 * - scan/connect/advertise permission, location requirement
 * - Scanner/Advertiser READY/FAILED
 * - nearby GeoRescuX device count
 * - per-peer connection state, GATT service discovered, RX/TX found,
 *   notifications enabled
 * - last packet sent / last packet received / last BLE error
 * - live "GeoRescueX-BLE" event log
 *
 * The Send Test Packet button emits the GEORESCUEX_BLE_TEST packet — the
 * FIRST acceptance criterion of the subsystem (A -> B before any SOS).
 */
class BleDiagnosticsActivity : AppCompatActivity() {

    private val appContainer by lazy { (application as GeoRescuXApplication).appContainer }
    private val bleManager get() = appContainer.bleManager

    private lateinit var statusText: TextView
    private lateinit var peersText: TextView
    private lateinit var lastSentText: TextView
    private lateinit var lastReceivedText: TextView
    private lateinit var receivedLogText: TextView
    private lateinit var eventLogText: TextView
    private lateinit var errorText: TextView
    private lateinit var testMessageInput: EditText
    private lateinit var grantPermissionsButton: Button
    // Nearby-device tappable list (fix: discovered devices had no click handler)
    private lateinit var nearbyDeviceButtons: LinearLayout
    private lateinit var nearbyDevicesHint: TextView

    private val refreshHandler = Handler(Looper.getMainLooper())
    private val refreshRunnable = object : Runnable {
        override fun run() {
            refreshStatus()
            refreshHandler.postDelayed(this, REFRESH_INTERVAL_MS)
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { _ ->
        // Re-evaluate after the grant round; the manager will initialize on
        // the next Start action if everything is in place.
        bleManager.initialize()
        refreshStatus()
    }

    private val enableBluetoothLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { _ ->
        bleManager.initialize()
        refreshStatus()
    }

    /** Received test packets (display only; persistence happens in AppContainer). */
    private val receivedTestPackets = ArrayDeque<GeoRescueBlePacket>()

    private val packetDisplayListener = object : com.example.georescux.data.ble.GeoRescueBleManager.PacketListener {
        override fun onPacketAccepted(packet: GeoRescueBlePacket, fromPeerId: String?) {
            if (fromPeerId == null) return
            runOnUiThread {
                if (packet.type == GeoRescueBlePacketType.TEST) {
                    receivedTestPackets.addLast(packet)
                    while (receivedTestPackets.size > MAX_DISPLAYED_PACKETS) {
                        receivedTestPackets.removeFirst()
                    }
                    renderReceivedPackets()
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_ble_diagnostics)

        statusText = findViewById(R.id.textBleStatusMatrix)
        peersText = findViewById(R.id.textBlePeers)
        lastSentText = findViewById(R.id.textLastSent)
        lastReceivedText = findViewById(R.id.textLastReceived)
        receivedLogText = findViewById(R.id.textReceivedPackets)
        eventLogText = findViewById(R.id.textEventLog)
        errorText = findViewById(R.id.textLastError)
        testMessageInput = findViewById(R.id.inputTestMessage)
        grantPermissionsButton = findViewById(R.id.buttonGrantPermissions)
        nearbyDeviceButtons = findViewById(R.id.layoutNearbyDeviceButtons)
        nearbyDevicesHint = findViewById(R.id.textNearbyDevicesHint)

        grantPermissionsButton.setOnClickListener {
            requestBlePermissions()
        }
        findViewById<Button>(R.id.buttonStartAdvertise).setOnClickListener {
            ensureInitialized()
            bleManager.startAdvertising()
            refreshStatus()
        }
        findViewById<Button>(R.id.buttonStopAdvertise).setOnClickListener {
            bleManager.stopAdvertising()
            refreshStatus()
        }
        findViewById<Button>(R.id.buttonStartScan).setOnClickListener {
            ensureInitialized()
            bleManager.startScanning()
            refreshStatus()
        }
        findViewById<Button>(R.id.buttonStopScan).setOnClickListener {
            bleManager.stopScanning()
            refreshStatus()
        }
        findViewById<Button>(R.id.buttonSendTest).setOnClickListener {
            sendTestPacket()
        }
        findViewById<Button>(R.id.buttonDisconnect).setOnClickListener {
            bleManager.diagnostics().peers.forEach { peer ->
                bleManager.disconnectPeer(peer.address)
            }
            refreshStatus()
        }

        bleManager.addPacketListener(packetDisplayListener)
        bleManager.initialize()
        renderReceivedPackets()
        refreshStatus()
    }

    override fun onResume() {
        super.onResume()
        refreshHandler.postDelayed(refreshRunnable, 0)
        GeoRescueBleDiagnostics.addListener(logListener)
    }

    override fun onPause() {
        super.onPause()
        refreshHandler.removeCallbacks(refreshRunnable)
        GeoRescueBleDiagnostics.removeListener(logListener)
    }

    override fun onDestroy() {
        super.onDestroy()
        refreshHandler.removeCallbacks(refreshRunnable)
        GeoRescueBleDiagnostics.removeListener(logListener)
        bleManager.removePacketListener(packetDisplayListener)
    }

    private val logListener = object : GeoRescueBleDiagnostics.BleLogListener {
        override fun onBleEvent(event: GeoRescueBleDiagnostics.GeoRescueBleEvent) {
            runOnUiThread { appendEventLog(event) }
        }
    }

    private fun appendEventLog(event: GeoRescueBleDiagnostics.GeoRescueBleEvent) {
        val time = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.US)
            .format(java.util.Date(event.timestampMs))
        val line = "$time ${event.eventName} ${event.detail}".trim()
        eventLogText.append("$line\n")
    }

    private fun ensureInitialized() {
        if (!bleManager.initialized) {
            bleManager.initialize()
        }
    }

    private fun sendTestPacket() {
        ensureInitialized()
        val message = testMessageInput.text?.toString()?.takeIf { it.isNotBlank() }
            ?: DEFAULT_TEST_MESSAGE
        val decision = bleManager.sendTestPacket(message)
        val suffix = when (decision) {
            com.example.georescux.domain.ble.GeoRescueBleRelayDecision.ACCEPTED_FORWARDED -> "forwarded to peer(s)"
            com.example.georescux.domain.ble.GeoRescueBleRelayDecision.ACCEPTED_QUEUED -> "queued (no peer yet; will flush on connect)"
            else -> decision.name
        }
        testMessageInput.setText("")
        refreshStatus()
        errorText.text = "Test packet sent ($suffix)."
    }

    private fun requestBlePermissions() {
        val required = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_ADVERTISE,
                Manifest.permission.BLUETOOTH_CONNECT,
            )
        } else {
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION,
            )
        }
        val missing = required.filter {
            ContextCompat.checkSelfPermission(this, it) !=
                android.content.pm.PackageManager.PERMISSION_GRANTED
        }
        if (missing.isEmpty()) {
            // Everything granted — maybe Bluetooth itself is off.
            val readiness = bleManager.readiness
            if (readiness != null && !readiness.bluetoothEnabled) {
                try {
                    enableBluetoothLauncher.launch(Intent(android.bluetooth.BluetoothAdapter.ACTION_REQUEST_ENABLE))
                } catch (_: Exception) {
                }
            }
        } else {
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    /**
     * Rebuilds the tappable nearby-device list.
     *
     * This is the fix for the defect described in the task:
     * "Phone A can display Phone B in the nearby-device list, but tapping the row
     * produces no visual feedback and no connection activity."
     *
     * Root cause: the discovered-device data existed in [GeoRescueBleManager.DiagnosticsState]
     * but was only rendered as a plain count in [statusText]. There was NO per-row View
     * with a click listener wired to [GeoRescueBleManager.connectTo]. This method
     * creates one [Button] per discovered device. Tapping the button calls
     * [GeoRescueBleManager.connectTo] which triggers the full GATT client connection
     * flow: CONNECTING → CONNECTED → DISCOVERING_SERVICES → READY.
     */
    private fun refreshNearbyDevices(diagnostics: com.example.georescux.data.ble.GeoRescueBleManager.DiagnosticsState) {
        val nearby = diagnostics.nearbyDevices
        val peersByAddress = diagnostics.peers.associateBy { it.address }
        val now = System.currentTimeMillis()

        if (nearby.isEmpty()) {
            nearbyDeviceButtons.removeAllViews()
            nearbyDevicesHint.text = "Scanning\u2026 no nearby GeoRescuX devices yet."
            nearbyDevicesHint.visibility = View.VISIBLE
            return
        }

        nearbyDevicesHint.text = "${nearby.size} device(s) found. Tap a row to connect."
        nearbyDevicesHint.visibility = View.VISIBLE

        val currentAddresses = nearby.map { it.address }.toSet()
        // Prune views for devices no longer in the nearby list
        for (i in nearbyDeviceButtons.childCount - 1 downTo 0) {
            val child = nearbyDeviceButtons.getChildAt(i)
            val addr = child.tag as? String
            if (addr == null || addr !in currentAddresses) {
                nearbyDeviceButtons.removeViewAt(i)
            }
        }

        nearby.sortedByDescending { it.rssi }.forEach { device ->
            val peer = peersByAddress[device.address]
            val isConnecting = peer?.state == com.example.georescux.domain.ble.GeoRescueBleState.CONNECTING ||
                peer?.state == com.example.georescux.domain.ble.GeoRescueBleState.DISCOVERING_SERVICES
            val isReady = peer?.state == com.example.georescux.domain.ble.GeoRescueBleState.READY
            val isConnected = peer?.state == com.example.georescux.domain.ble.GeoRescueBleState.CONNECTED
            val isLive = isConnecting || isReady || isConnected

            val ageSec = ((now - device.lastSeenMs) / 1000L).coerceAtLeast(0L)
            val isStale = ageSec >= 20L

            val buttonText = buildString {
                append(device.address)
                append("  RSSI: ${device.rssi} dBm")
                when {
                    isReady -> append("  \u2713 linked")
                    isConnected -> append("  connected")
                    isConnecting -> append("  connecting\u2026")
                    isStale -> append("  (stale, ${ageSec}s ago)")
                }
            }

            var btn = nearbyDeviceButtons.findViewWithTag<Button>(device.address)
            if (btn == null) {
                btn = Button(this).apply {
                    id = View.generateViewId()
                    tag = device.address
                    contentDescription = "connect_to_${device.address.replace(':', '_')}"
                    textSize = 12f
                    isAllCaps = false
                    backgroundTintList = null
                    val lp = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    ).apply { topMargin = (4 * resources.displayMetrics.density).toInt() }
                    layoutParams = lp
                    nearbyDeviceButtons.addView(this)
                }
            }

            btn.text = buttonText
            btn.isEnabled = !isLive
            btn.setBackgroundResource(
                if (isLive) R.drawable.bg_button_secondary
                else R.drawable.bg_button_violet
            )

            btn.setOnClickListener {
                btn.isEnabled = false
                btn.text = "${device.address}  connecting\u2026"
                btn.setBackgroundResource(R.drawable.bg_button_secondary)

                val started = bleManager.connectTo(device.address)
                if (started) {
                    GeoRescueBleDiagnostics.info(
                        GeoRescueBleDiagnostics.GATT_CONNECTING,
                        "source=UI_TAP address=${device.address}"
                    )
                    errorText.text = "Connecting to ${device.address}\u2026"
                } else {
                    errorText.text = "connectTo(${device.address}) returned false — check if already connected or BLE not ready."
                }
                refreshStatus()
            }
        }
    }

    private fun refreshStatus() {
        val diagnostics = bleManager.diagnostics()
        val readiness = diagnostics.readiness
        val permissions = readiness?.permissionReport

        fun yesNo(value: Boolean) = if (value) "YES" else "NO"
        fun granted(value: com.example.georescux.domain.ble.GeoRescueBlePermissions.BlePermissionState?) =
            when (value) {
                com.example.georescux.domain.ble.GeoRescueBlePermissions.BlePermissionState.GRANTED -> "GRANTED"
                com.example.georescux.domain.ble.GeoRescueBlePermissions.BlePermissionState.DENIED -> "DENIED"
                null -> "UNKNOWN"
                else -> "UNKNOWN"
            }

        statusText.text = buildString {
            appendLine("Bluetooth supported: ${yesNo(readiness?.bluetoothSupported ?: false)}")
            appendLine("Bluetooth enabled: ${yesNo(readiness?.bluetoothEnabled ?: false)}")
            appendLine("BLE supported: ${yesNo(readiness?.bleSupported ?: false)}")
            appendLine("Scan permission: ${granted(permissions?.scan)}")
            appendLine("Connect permission: ${granted(permissions?.connect)}")
            appendLine("Advertise permission: ${granted(permissions?.advertise)}")
            appendLine("Location requirement: ${granted(permissions?.location)}")
            appendLine("Scanner: ${if (diagnostics.scanning) "SCANNING" else if (readiness?.bluetoothEnabled == true) "READY" else "FAILED"}")
            appendLine("Advertiser: ${if (diagnostics.advertising) "ADVERTISING" else if (readiness?.bluetoothEnabled == true) "READY" else "FAILED"}")
            appendLine("Subsystem: ${if (diagnostics.initialized) "READY" else "NOT READY"}")
            appendLine("Nearby GeoRescuX devices: ${diagnostics.nearbyCount}")
            appendLine("Outbound queued (store-and-forward): ${diagnostics.pendingOutbound}")
            appendLine("Seen packets (dedup cache): ${diagnostics.seenPackets}")
        }

        val peers = diagnostics.peers
        peersText.text = if (peers.isEmpty()) {
            "No peer connections yet."
        } else {
            peers.joinToString("\n") { peer ->
                buildString {
                    appendLine("[${peer.role}] ${peer.address}")
                    appendLine("  state=${peer.state}")
                    appendLine("  GATT service discovered: ${yesNo(peer.serviceDiscovered)}")
                    appendLine("  RX characteristic: ${yesNo(peer.rxFound)}")
                    appendLine("  TX characteristic: ${yesNo(peer.txFound)}")
                    append("  Notifications enabled: ${yesNo(peer.notificationsEnabled)}")
                }
            }
        }

        lastSentText.text = diagnostics.lastPacketSent?.let { summarize(it) } ?: "—"
        lastReceivedText.text = diagnostics.lastPacketReceived?.let { summarize(it) } ?: "—"
        errorText.text = diagnostics.lastError ?: "No BLE error recorded."

        grantPermissionsButton.visibility =
            if (permissions?.allRequiredGranted == true && readiness?.bluetoothEnabled == true) {
                View.GONE
            } else {
                View.VISIBLE
            }

        // Rebuild the tappable discovered-device list on every refresh cycle.
        refreshNearbyDevices(diagnostics)
    }

    private fun renderReceivedPackets() {
        receivedLogText.text = if (receivedTestPackets.isEmpty()) {
            "No test packets received yet."
        } else {
            receivedTestPackets.toList().joinToString("\n\n") { packet ->
                buildString {
                    appendLine("GEORESCUEX_BLE_TEST RECEIVED ✔")
                    appendLine("packetId: ${packet.packetId}")
                    appendLine("from: ${packet.originDeviceId}")
                    appendLine("message: ${packet.payload["message"] ?: ""}")
                }.trimEnd()
            }
        }
    }

    private fun summarize(packet: GeoRescueBlePacket): String =
        buildString {
            append("${packet.type.wireName} ttl=${packet.ttl} id=${packet.packetId.take(13)}…")
            packet.status?.let { append(" status=$it") }
        }

    private companion object {
        const val REFRESH_INTERVAL_MS = 1000L
        const val MAX_DISPLAYED_PACKETS = 8
        const val DEFAULT_TEST_MESSAGE = "GEORESCUEX_BLE_TEST"
    }
}
