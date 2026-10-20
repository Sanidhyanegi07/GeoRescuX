import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GeoRescuX.Prototype 1.0

Rectangle {
    id: userHomeScreen
    color: "#080C14"

    signal requestResolveEmergency()

    ScrollView {
        anchors.fill: parent
        anchors.margins: 14
        ColumnLayout {
            width: parent.width
            spacing: 14

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                radius: 12
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#1E293B" }
                    GradientStop { position: 1.0; color: "#0F172A" }
                }
                border.color: "#334155"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: "Welcome, " + sessionModel.currentUserName; color: "#FFFFFF"; font.pixelSize: 16; font.weight: Font.Bold }
                        Text { text: "Citizen Responder | " + sessionModel.currentUserEmail; color: "#38BDF8"; font.pixelSize: 11 }
                    }
                    Rectangle {
                        width: 80; height: 26; radius: 13
                        color: "#064E3B"; border.color: "#059669"; border.width: 1
                        Text { anchors.centerIn: parent; text: "VERIFIED"; color: "#A7F3D0"; font.pixelSize: 9; font.weight: Font.Bold }
                    }
                }
            }

            // -----------------------------------------------------
            // Life-Safety Emergency SOS Engine Card
            // -----------------------------------------------------
            Rectangle {
                visible: !emergencyService.hasActiveEmergency
                Layout.fillWidth: true
                Layout.preferredHeight: sosActiveCol.implicitHeight + 24
                radius: 12
                color: "#181014"
                border.color: "#7F1D1D"
                border.width: 1

                ColumnLayout {
                    id: sosActiveCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "🚨 Life-Safety Emergency SOS Engine"
                            color: "#F8FAFC"
                            font.pixelSize: 14
                            font.weight: Font.Bold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "Local SQLite V2"
                            color: "#34D399"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }

                    Text {
                        text: "Triggering SOS generates an immutable emergency record in local SQLite storage. Single-active-emergency invariant is enforced by SQLite partial unique index."
                        color: "#94A3B8"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        TextField {
                            id: sosNoteInput
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            placeholderText: "Emergency Note (Optional, e.g. Injury, Flood - max 120 chars)"
                            maximumLength: 120
                            color: "#F8FAFC"
                            placeholderTextColor: "#64748B"
                            background: Rectangle {
                                radius: 6
                                color: "#0F172A"
                                border.color: sosNoteInput.activeFocus ? "#EF4444" : "#334155"
                                border.width: 1
                            }
                            font.pixelSize: 11
                        }

                        Text {
                            text: (120 - sosNoteInput.text.length) + " left"
                            color: "#64748B"
                            font.pixelSize: 10
                        }
                    }

                    // -----------------------------------------------------
                    // Two-Second Hold-to-Activate SOS Control (Phase 4 UX)
                    // -----------------------------------------------------
                    Rectangle {
                        id: sosHoldControl
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        radius: 10
                        color: isHolding ? "#450A0A" : "#1F0A0A"
                        border.color: isHolding ? "#EF4444" : "#991B1B"
                        border.width: isHolding ? 2 : 1
                        clip: true

                        property int holdDurationMs: 2000
                        property int holdElapsedMs: 0
                        property real holdProgress: 0.0
                        property bool isHolding: false
                        property bool activationFired: false

                        function startHold() {
                            if (emergencyService.hasActiveEmergency || activationFired) return;
                            isHolding = true;
                            holdElapsedMs = 0;
                            holdProgress = 0.0;
                            activationFired = false;
                            holdTimer.restart();
                        }

                        function cancelHold() {
                            holdTimer.stop();
                            isHolding = false;
                            holdElapsedMs = 0;
                            holdProgress = 0.0;
                            activationFired = false;
                        }

                        function completeHold() {
                            holdTimer.stop();
                            isHolding = false;
                            activationFired = true;
                            holdProgress = 1.0;
                            const note = sosNoteInput.text;
                            appState.activateSos(note);
                            sosNoteInput.text = "";
                            resetTimer.restart();
                        }

                        Timer {
                            id: holdTimer
                            interval: 25
                            repeat: true
                            onTriggered: {
                                if (!sosHoldControl.isHolding || emergencyService.hasActiveEmergency) {
                                    stop();
                                    return;
                                }
                                sosHoldControl.holdElapsedMs += 25;
                                sosHoldControl.holdProgress = Math.min(1.0, sosHoldControl.holdElapsedMs / sosHoldControl.holdDurationMs);

                                if (sosHoldControl.holdElapsedMs >= sosHoldControl.holdDurationMs) {
                                    sosHoldControl.completeHold();
                                }
                            }
                        }

                        Timer {
                            id: resetTimer
                            interval: 400
                            repeat: false
                            onTriggered: {
                                sosHoldControl.holdProgress = 0.0;
                                sosHoldControl.holdElapsedMs = 0;
                                sosHoldControl.activationFired = false;
                            }
                        }

                        // Dynamic Animated Fill Bar
                        Rectangle {
                            id: progressFill
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.width * sosHoldControl.holdProgress
                            radius: parent.radius
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#DC2626" }
                                GradientStop { position: 1.0; color: "#B91C1C" }
                            }
                            visible: sosHoldControl.holdProgress > 0.0
                        }

                        // Foreground Content
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 10

                            Text {
                                text: sosHoldControl.isHolding ? "⏳" : "🚨"
                                font.pixelSize: 18
                            }

                            ColumnLayout {
                                spacing: 1
                                Text {
                                    text: {
                                        if (sosHoldControl.activationFired) return "SOS ACTIVATION INITIATED";
                                        if (sosHoldControl.isHolding) {
                                            const remaining = Math.max(0.0, (sosHoldControl.holdDurationMs - sosHoldControl.holdElapsedMs) / 1000.0);
                                            return "HOLDING... " + remaining.toFixed(1) + "s (RELEASE TO CANCEL)";
                                        }
                                        return "PRESS AND HOLD 2s TO ACTIVATE SOS";
                                    }
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                }
                                Text {
                                    text: sosHoldControl.isHolding
                                        ? Math.round(sosHoldControl.holdProgress * 100) + "% complete — Release early to abort"
                                        : "Continuous 2.0-second press required to prevent accidental activation"
                                    color: sosHoldControl.isHolding ? "#FECACA" : "#94A3B8"
                                    font.pixelSize: 9
                                }
                            }
                        }

                        MouseArea {
                            id: holdMouseArea
                            anchors.fill: parent
                            enabled: !emergencyService.hasActiveEmergency
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true

                            onPressed: sosHoldControl.startHold()
                            onReleased: {
                                if (!sosHoldControl.activationFired) {
                                    sosHoldControl.cancelHold();
                                }
                            }
                            onCanceled: sosHoldControl.cancelHold()
                            onExited: {
                                if (sosHoldControl.isHolding && !sosHoldControl.activationFired) {
                                    sosHoldControl.cancelHold();
                                }
                            }
                        }

                        // Cancel hold on navigation or state change
                        Connections {
                            target: appState
                            function onNavigationChanged() { sosHoldControl.cancelHold(); }
                            function onEmergencyStateChanged() { sosHoldControl.cancelHold(); }
                        }
                    }
                }
            }

            // Active Emergency Card in UserHome
            Rectangle {
                visible: emergencyService.hasActiveEmergency
                Layout.fillWidth: true
                Layout.preferredHeight: activeCardCol.implicitHeight + 24
                radius: 12
                color: "#3B0D0D"
                border.color: "#EF4444"
                border.width: 1.5

                ColumnLayout {
                    id: activeCardCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "🚨 ACTIVE EMERGENCY IN PROGRESS"
                            color: "#FEE2E2"
                            font.pixelSize: 13
                            font.weight: Font.Bold
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            height: 20
                            width: actBadgeText.contentWidth + 12
                            radius: 10
                            color: "#7F1D1D"
                            border.color: "#DC2626"
                            border.width: 1
                            Text {
                                id: actBadgeText
                                anchors.centerIn: parent
                                text: "PERSISTED"
                                color: "#FECACA"
                                font.pixelSize: 9
                                font.weight: Font.Bold
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#7F1D1D"
                    }

                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        columnSpacing: 10
                        rowSpacing: 6

                        Text { text: "Emergency ID:"; color: "#FCA5A5"; font.pixelSize: 11; font.weight: Font.Bold }
                        Text { text: emergencyService.activeEmergencyId; color: "#FFFFFF"; font.pixelSize: 11; font.family: "monospace" }

                        Text { text: "Started UTC:"; color: "#FCA5A5"; font.pixelSize: 11; font.weight: Font.Bold }
                        Text { text: emergencyService.activeStartedTimeString; color: "#FFFFFF"; font.pixelSize: 11 }

                        Text { text: "Location:"; color: "#FCA5A5"; font.pixelSize: 11; font.weight: Font.Bold }
                        Text { text: emergencyService.activeLocationString; color: "#FDE68A"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                        Text { text: "GPS Tracking:"; color: "#FCA5A5"; font.pixelSize: 11; font.weight: Font.Bold }
                        Text { text: appState.locationService ? (appState.locationService.trackingStateName + " (Active SOS Policy: 2s / 2m)") : "Inactive"; color: "#93C5FD"; font.pixelSize: 11 }

                        Text { text: "Note:"; color: "#FCA5A5"; font.pixelSize: 11; font.weight: Font.Bold }
                        Text { text: emergencyService.activeNote.length > 0 ? emergencyService.activeNote : "None provided"; color: "#FFFFFF"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        background: Rectangle {
                            radius: 8
                            color: "#1E293B"
                            border.color: "#F87171"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: "Resolve & Complete Emergency"
                            color: "#FECACA"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: userHomeScreen.requestResolveEmergency()
                    }
                }
            }

            // Readiness Overview Grid
            GridLayout {
                columns: 2
                Layout.fillWidth: true
                columnSpacing: 10
                rowSpacing: 10

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 86; radius: 10; color: "#111827"; border.color: "#1F2937"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 2
                        Text { text: "🗺️ Offline Region"; color: "#94A3B8"; font.pixelSize: 11 }
                        Text { text: "Uttarakhand"; color: "#F8FAFC"; font.pixelSize: 13; font.weight: Font.Bold }
                        Text { text: "Topological Graph Ready"; color: "#10B981"; font.pixelSize: 10 }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 86; radius: 10; color: "#111827"; border.color: "#1F2937"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 2
                        Text { text: "💾 Local Storage"; color: "#94A3B8"; font.pixelSize: 11 }
                        Text { text: appState.persistenceStatusName; color: "#F8FAFC"; font.pixelSize: 12; font.weight: Font.Bold }
                        Text { text: "Schema V2 Active"; color: "#38BDF8"; font.pixelSize: 10 }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 86; radius: 10; color: "#111827"; border.color: "#1F2937"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 2
                        Text { text: "📡 BLE Mesh"; color: "#94A3B8"; font.pixelSize: 11 }
                        Text { text: "Not Initialized"; color: "#F8FAFC"; font.pixelSize: 13; font.weight: Font.Bold }
                        Text { text: "Deferred to Phase 7"; color: "#F59E0B"; font.pixelSize: 10 }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 86; radius: 10; color: "#111827"; border.color: "#1F2937"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 2
                        Text { text: "☁️ Cloud Sync"; color: "#94A3B8"; font.pixelSize: 11 }
                        Text { text: "Offline Only"; color: "#F8FAFC"; font.pixelSize: 12; font.weight: Font.Bold }
                        Text { text: "Deferred to Phase 8"; color: "#F59E0B"; font.pixelSize: 10 }
                    }
                }
            }

            // Quick Navigation Actions
            Text { text: "Quick Portals & Tools"; color: "#CBD5E1"; font.pixelSize: 13; font.weight: Font.DemiBold }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    Layout.fillWidth: true; Layout.preferredHeight: 46
                    background: Rectangle { radius: 8; color: "#0284C7" }
                    contentItem: Text { text: "🧭 Safe Route"; color: "#FFFFFF"; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: appState.navigateTo("UserSafeRoute")
                }

                Button {
                    Layout.fillWidth: true; Layout.preferredHeight: 46
                    background: Rectangle { radius: 8; color: "#0F172A"; border.color: "#38BDF8"; border.width: 1 }
                    contentItem: Text { text: "🔬 C++ Lab"; color: "#38BDF8"; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: appState.navigateTo("PrototypeLab")
                }
            }
        }
    }
}
