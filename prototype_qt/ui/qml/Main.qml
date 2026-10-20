import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GeoRescuX.Prototype 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 520
    height: 900
    minimumWidth: 380
    minimumHeight: 680
    title: qsTr("GeoRescuX — Autonomous Disaster Emergency Response (C++20/Qt)")
    color: "#080C14" // Deep midnight obsidian

    // -------------------------------------------------------------------------
    // Top Bar with System Badges and Authorized Role Indicator
    // -------------------------------------------------------------------------
    header: Rectangle {
        height: 72
        color: "#0F172A"
        border.color: "#1E293B"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 6
            anchors.bottomMargin: 6
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    width: 36
                    height: 36
                    radius: 8
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#0284C7" }
                        GradientStop { position: 1.0; color: "#0369A1" }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "🛡️"
                        font.pixelSize: 18
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text {
                        text: "GeoRescuX"
                        color: "#F8FAFC"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                    }
                    Text {
                        text: appState.activePortalName
                        color: sessionModel.isAdmin ? "#F59E0B" : (sessionModel.isAuthenticated ? "#38BDF8" : "#94A3B8")
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }

                // Authority / Role Badge
                Rectangle {
                    height: 24
                    width: roleBadgeText.contentWidth + 14
                    radius: 12
                    color: sessionModel.isAdmin ? "#78350F" : (sessionModel.isAuthenticated ? "#064E3B" : "#1E293B")
                    border.color: sessionModel.isAdmin ? "#D97706" : (sessionModel.isAuthenticated ? "#059669" : "#334155")
                    border.width: 1
                    Text {
                        id: roleBadgeText
                        anchors.centerIn: parent
                        text: sessionModel.roleName
                        color: sessionModel.isAdmin ? "#FDE68A" : (sessionModel.isAuthenticated ? "#A7F3D0" : "#94A3B8")
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                }

                // Emergency Status Badge (Strictly Inactive until Phase 3)
                Rectangle {
                    height: 24
                    width: emergencyBadgeText.contentWidth + 14
                    radius: 12
                    color: appState.isEmergencyActive ? "#7F1D1D" : "#111827"
                    border.color: appState.isEmergencyActive ? "#DC2626" : "#334155"
                    border.width: 1
                    Text {
                        id: emergencyBadgeText
                        anchors.centerIn: parent
                        text: "SOS: " + appState.emergencyStatusName
                        color: appState.isEmergencyActive ? "#FECACA" : "#64748B"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                }
            }

            // Dev / Testing Facility Role Switcher Bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                radius: 4
                color: "#182234"
                border.color: "#2E3D56"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    Text {
                        text: "[DEV TEST FACILITY]:"
                        color: "#F59E0B"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        id: devUserBtn
                        background: Rectangle { color: "transparent" }
                        contentItem: Text {
                            text: "Login Citizen"
                            color: sessionModel.currentRole === 1 ? "#38BDF8" : "#94A3B8"
                            font.pixelSize: 10
                            font.underline: devUserBtn.hovered
                        }
                        onClicked: appState.loginDevelopmentUser("Aarav Sharma", "aarav@georescux.org")
                    }

                    Text { text: "|"; color: "#475569"; font.pixelSize: 10 }

                    Button {
                        id: devAdminBtn
                        background: Rectangle { color: "transparent" }
                        contentItem: Text {
                            text: "Login Admin"
                            color: sessionModel.currentRole === 2 ? "#F59E0B" : "#94A3B8"
                            font.pixelSize: 10
                            font.underline: devAdminBtn.hovered
                        }
                        onClicked: appState.loginDevelopmentAdmin("Dr. Neha Verma", "admin@georescux.org")
                    }

                    Text { text: "|"; color: "#475569"; font.pixelSize: 10 }

                    Button {
                        id: devLogoutBtn
                        visible: sessionModel.isAuthenticated
                        background: Rectangle { color: "transparent" }
                        contentItem: Text {
                            text: "Sign Out"
                            color: "#EF4444"
                            font.pixelSize: 10
                            font.underline: devLogoutBtn.hovered
                        }
                        onClicked: appState.logout()
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Main Content Layout
    // -------------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Subsystem / Navigation Pill Rail
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff

            RowLayout {
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10
                anchors.rightMargin: 10

                Repeater {
                    model: {
                        if (!sessionModel.isAuthenticated) {
                            return [
                                { id: "Welcome", title: "Welcome", icon: "👋" },
                                { id: "Register", title: "Register", icon: "📝" },
                                { id: "PrototypeLab", title: "C++ Engine Lab", icon: "🔬" }
                            ];
                        }
                        if (sessionModel.isAdmin) {
                            return [
                                { id: "AdminDashboard", title: "Dashboard", icon: "📊" },
                                { id: "AdminMap", title: "Admin Map", icon: "🗺️" },
                                { id: "AdminSos", title: "SOS Queue", icon: "🚨" },
                                { id: "AdminIncidents", title: "Incidents", icon: "⚠️" },
                                { id: "AdminHazards", title: "Hazards", icon: "🚧" },
                                { id: "AdminEvents", title: "Events", icon: "📅" },
                                { id: "AdminRoutes", title: "Routes", icon: "🛣️" },
                                { id: "AdminUsers", title: "Users", icon: "👥" },
                                { id: "AdminVerification", title: "Verification", icon: "🪪" },
                                { id: "AdminAlerts", title: "Alerts Broadcast", icon: "📢" },
                                { id: "PrototypeLab", title: "C++ Engine Lab", icon: "🔬" }
                            ];
                        }
                        return [
                            { id: "UserHome", title: "Home", icon: "🏠" },
                            { id: "UserMap", title: "Offline Map", icon: "🗺️" },
                            { id: "UserSafeRoute", title: "Safe Route", icon: "🧭" },
                            { id: "UserAlerts", title: "Alerts", icon: "🔔" },
                            { id: "UserContacts", title: "Contacts", icon: "📞" },
                            { id: "UserProfile", title: "Profile", icon: "👤" },
                            { id: "UserHelp", title: "Help Center", icon: "❓" },
                            { id: "PrototypeLab", title: "C++ Engine Lab", icon: "🔬" }
                        ];
                    }

                    delegate: Rectangle {
                        height: 34
                        width: navRow.contentWidth + 20
                        radius: 17
                        color: appState.currentNavigation === modelData.id ? "#1E293B" : "#0B111E"
                        border.color: appState.currentNavigation === modelData.id ? (sessionModel.isAdmin ? "#F59E0B" : "#38BDF8") : "#1E293B"
                        border.width: appState.currentNavigation === modelData.id ? 1.5 : 1

                        RowLayout {
                            id: navRow
                            anchors.centerIn: parent
                            spacing: 4
                            Text { text: modelData.icon; font.pixelSize: 11 }
                            Text {
                                text: modelData.title
                                color: appState.currentNavigation === modelData.id ? "#FFFFFF" : "#94A3B8"
                                font.pixelSize: 11
                                font.weight: appState.currentNavigation === modelData.id ? Font.Bold : Font.Normal
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appState.navigateTo(modelData.id)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#1E293B"
        }

        // ---------------------------------------------------------------------
        // Persistent Active SOS Emergency Banner
        // Authoritative: Drives all Citizen User screens while emergency is active
        // ---------------------------------------------------------------------
        Rectangle {
            id: activeSosBanner
            visible: sessionModel.isAuthenticated && !sessionModel.isAdmin && emergencyService.hasActiveEmergency
            Layout.fillWidth: true
            Layout.preferredHeight: bannerContentCol.implicitHeight + 16
            color: "#7F1D1D" // Urgency red
            border.color: "#EF4444"
            border.width: 1

            ColumnLayout {
                id: bannerContentCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: "#DC2626"
                        Text {
                            anchors.centerIn: parent
                            text: "🚨"
                            font.pixelSize: 14
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        RowLayout {
                            spacing: 6
                            Text {
                                text: "SOS EMERGENCY ACTIVE"
                                color: "#FFFFFF"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                            }
                            Rectangle {
                                height: 16
                                width: emergencyIdText.contentWidth + 8
                                radius: 4
                                color: "#991B1B"
                                Text {
                                    id: emergencyIdText
                                    anchors.centerIn: parent
                                    text: emergencyService.activeEmergencyId
                                    color: "#FECACA"
                                    font.pixelSize: 9
                                    font.family: "monospace"
                                }
                            }
                        }

                        Text {
                            text: "Started: " + emergencyService.activeStartedTimeString + " | " + emergencyService.activeLocationString
                            color: "#FCA5A5"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    Button {
                        id: resolveSosBtn
                        Layout.preferredHeight: 30
                        background: Rectangle {
                            radius: 6
                            color: "#991B1B"
                            border.color: "#F87171"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: "Resolve SOS"
                            color: "#FFFFFF"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: confirmResolveDialog.open()
                    }
                }

                // Optional note preview in banner if present
                RowLayout {
                    Layout.fillWidth: true
                    visible: emergencyService.activeNote.length > 0
                    spacing: 4
                    Text {
                        text: "Note:"
                        color: "#FECACA"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                    Text {
                        text: emergencyService.activeNote
                        color: "#FEE2E2"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Active Screen View Container
        StackLayout {
            id: mainStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: {
                const nav = appState.currentNavigation;
                if (nav === "Welcome") return 0;
                if (nav === "Register") return 1;
                if (nav === "UserHome") return 2;
                if (nav === "UserMap") return 3;
                if (nav === "UserSafeRoute") return 4;
                if (nav === "UserAlerts") return 5;
                if (nav === "UserContacts") return 6;
                if (nav === "UserProfile") return 7;
                if (nav === "UserHelp") return 8;
                if (nav === "AdminDashboard") return 9;
                if (nav === "AdminMap") return 10;
                if (nav === "AdminSos") return 11;
                if (nav === "AdminIncidents") return 12;
                if (nav === "AdminHazards") return 13;
                if (nav === "AdminEvents") return 14;
                if (nav === "AdminRoutes") return 15;
                if (nav === "AdminUsers") return 16;
                if (nav === "AdminVerification") return 17;
                if (nav === "AdminAlerts") return 18;
                if (nav === "PrototypeLab") return 19;
                return 0;
            }

            // 0: Welcome / Sign-In Placeholder
            Rectangle {
                color: "#080C14"
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, 420)
                    spacing: 16

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 72; height: 72; radius: 20
                        color: "#0F172A"; border.color: "#1E293B"; border.width: 1
                        Text { anchors.centerIn: parent; text: "🛡️"; font.pixelSize: 36 }
                    }

                    Text {
                        text: "Welcome to GeoRescuX"
                        color: "#F8FAFC"
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Autonomous Offline-First Disaster Response & Evacuation System"
                        color: "#94A3B8"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        radius: 12
                        color: "#0F172A"
                        border.color: "#1E293B"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8
                            Text { text: "PHASE 2 AUTHENTICATION PLACEHOLDER"; color: "#38BDF8"; font.pixelSize: 10; font.weight: Font.Bold }
                            Text {
                                text: "Production Firebase Authentication is scheduled for Phase 7. Use the visibly marked development login buttons above or below to test Citizen User or Admin Portal navigation."
                                color: "#CBD5E1"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true
                            }
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        background: Rectangle {
                            radius: 8
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#0284C7" }
                                GradientStop { position: 1.0; color: "#0369A1" }
                            }
                        }
                        contentItem: Text {
                            text: "▶  Enter Citizen Portal (Test Session)"
                            color: "#FFFFFF"; font.pixelSize: 12; font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: appState.loginDevelopmentUser("Aarav Sharma", "aarav@georescux.org")
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        background: Rectangle {
                            radius: 8
                            color: "#78350F"
                            border.color: "#D97706"; border.width: 1
                        }
                        contentItem: Text {
                            text: "🛡️  Enter Admin Command Portal (Test Session)"
                            color: "#FDE68A"; font.pixelSize: 12; font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: appState.loginDevelopmentAdmin("Dr. Neha Verma", "admin@georescux.org")
                    }
                }
            }

            // 1: Register Placeholder
            Rectangle {
                color: "#080C14"
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, 420)
                    spacing: 12
                    Text { text: "Citizen Registration (Phase 2 Placeholder)"; color: "#F8FAFC"; font.pixelSize: 18; font.weight: Font.Bold }
                    Text { text: "Status: Live registration is deferred to Phase 7. Local session caching is active via SQLite Schema V1."; color: "#94A3B8"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }

            // 2: User Home (Citizen SOS & Life Safety Engine)
            CitizenHomeView {
                onRequestResolveEmergency: confirmResolveDialog.open()
            }


            // 3: User Map — Real Offline Regional Map View (Phases 5 & 6)
            CitizenMapView {}

            // 4: User Safe Route — Offline Evacuation Pathfinding (Phase 7)
            CitizenSafeRouteView {}


            // 5: User Alerts Placeholder
            Rectangle {
                color: "#080C14"
                ColumnLayout {
                    anchors.centerIn: parent; width: Math.min(parent.width - 40, 420); spacing: 10
                    Text { text: "🔔 Emergency Alerts (Phase 7 Placeholder)"; color: "#F8FAFC"; font.pixelSize: 16; font.weight: Font.Bold }
                    Text { text: "Status: No active alerts recorded. Verified zero fake data policy active."; color: "#94A3B8"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }

            // 6: User Contacts Placeholder
            Rectangle {
                color: "#080C14"
                ColumnLayout {
                    anchors.centerIn: parent; width: Math.min(parent.width - 40, 420); spacing: 10
                    Text { text: "📞 Emergency Contacts (Phase 7 Placeholder)"; color: "#F8FAFC"; font.pixelSize: 16; font.weight: Font.Bold }
                    Text { text: "Status: Local SQLite contact schema and primary-contact policy integration scheduled for Phase 7."; color: "#94A3B8"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }

            // 7: User Profile Placeholder
            Rectangle {
                color: "#080C14"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                    Text { text: "👤 Responder Profile"; color: "#F8FAFC"; font.pixelSize: 18; font.weight: Font.Bold }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 120; radius: 10; color: "#0F172A"; border.color: "#1E293B"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 6
                            Text { text: "Name: " + sessionModel.currentUserName; color: "#FFFFFF"; font.pixelSize: 13 }
                            Text { text: "Email: " + sessionModel.currentUserEmail; color: "#94A3B8"; font.pixelSize: 12 }
                            Text { text: "Role: " + sessionModel.roleName; color: "#38BDF8"; font.pixelSize: 12 }
                            Text { text: "Verification: Verified Citizen Identity"; color: "#10B981"; font.pixelSize: 12 }
                        }
                    }
                }
            }

            // 8: User Help Center
            Rectangle {
                color: "#080C14"
                ScrollView {
                    anchors.fill: parent; anchors.margins: 14
                    ColumnLayout {
                        width: parent.width; spacing: 10
                        Text { text: "❓ GeoRescuX Help Center & FAQs"; color: "#F8FAFC"; font.pixelSize: 16; font.weight: Font.Bold }
                        Repeater {
                            model: [
                                {
                                    q: "How do I activate an SOS emergency?",
                                    a: "Press and hold the SOS button continuously for 2.0 seconds. A real-time progress bar and countdown display pending activation. If you release before 2 seconds, leave the screen, or cancel, the activation is immediately aborted without triggering an emergency."
                                },
                                {
                                    q: "Why is a two-second hold required?",
                                    a: "The two-second hold prevents accidental activations from unintended screen taps or pocket presses while still enabling rapid response during life-safety events. Releasing early resets the timer completely."
                                },
                                {
                                    q: "Are live GPS, BLE mesh, and cloud sync operational?",
                                    a: "Phase 4 implements the local emergency lifecycle and durable SQLite persistence. Live GPS tracking, BLE mesh radio relay, and Firebase cloud sync are explicitly deferred to subsequent phases and are not yet operational."
                                },
                                {
                                    q: "How is my emergency data secured and persisted?",
                                    a: "All emergency records and state transitions are durably recorded in local SQLite tables with strict authenticated user ownership validation and atomic transactions."
                                }
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(90, faqCol.implicitHeight + 20)
                                radius: 8
                                color: "#0F172A"
                                border.color: "#1E293B"
                                ColumnLayout {
                                    id: faqCol
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 4
                                    Text {
                                        text: modelData.q
                                        color: "#38BDF8"
                                        font.pixelSize: 11
                                        font.weight: Font.Bold
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: modelData.a
                                        color: "#CBD5E1"
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 9: Admin Dashboard
            Rectangle {
                color: "#080C14"
                ScrollView {
                    anchors.fill: parent; anchors.margins: 14
                    ColumnLayout {
                        width: parent.width; spacing: 12
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 70; radius: 10; color: "#78350F"; border.color: "#D97706"; border.width: 1
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 12
                                ColumnLayout {
                                    Text { text: "ADMINISTRATIVE COMMAND CONSOLE"; color: "#FDE68A"; font.pixelSize: 14; font.weight: Font.Bold }
                                    Text { text: "Authorized Administrator: " + sessionModel.currentUserName; color: "#FEF3C7"; font.pixelSize: 11 }
                                }
                            }
                        }
                        Text { text: "Operational Command Modules"; color: "#CBD5E1"; font.pixelSize: 13; font.weight: Font.DemiBold }
                        GridLayout {
                            columns: 2; Layout.fillWidth: true; columnSpacing: 10; rowSpacing: 10
                            Repeater {
                                model: [
                                    { title: "Active SOS Queue", code: "AdminSos", icon: "🚨", status: "0 Active (Phase 3)" },
                                    { title: "Regional Map Intel", code: "AdminMap", icon: "🗺️", status: "Uttarakhand Active" },
                                    { title: "Incident Triage", code: "AdminIncidents", icon: "⚠️", status: "0 Pending (Phase 8)" },
                                    { title: "Hazard Exclusion", code: "AdminHazards", icon: "🚧", status: "0 Active Zones (Phase 8)" },
                                    { title: "Evacuation Routes", code: "AdminRoutes", icon: "🛣️", status: "Offline Graph Ready" },
                                    { title: "Identity Review", code: "AdminVerification", icon: "🪪", status: "0 Pending Review (Phase 8)" }
                                ]
                                delegate: Rectangle {
                                    Layout.fillWidth: true; Layout.preferredHeight: 76; radius: 8; color: "#0F172A"; border.color: "#1E293B"
                                    ColumnLayout {
                                        anchors.fill: parent; anchors.margins: 10; spacing: 2
                                        Text { text: modelData.icon + " " + modelData.title; color: "#F8FAFC"; font.pixelSize: 11; font.weight: Font.Bold }
                                        Text { text: modelData.status; color: "#F59E0B"; font.pixelSize: 10 }
                                    }
                                    MouseArea {
                                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: appState.navigateTo(modelData.code)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 10: Admin Map Placeholder
            Rectangle { color: "#080C14"; Text { anchors.centerIn: parent; text: "Admin Map Intelligence (Scheduled Phase 8)"; color: "#F59E0B" } }
            // 11: Admin SOS Placeholder
            Rectangle { color: "#080C14"; Text { anchors.centerIn: parent; text: "Admin SOS Prioritization Queue (Scheduled Phase 8)"; color: "#F59E0B" } }
            // 12: Admin Incidents Placeholder
            Rectangle { color: "#080C14"; Text { anchors.centerIn: parent; text: "Admin Incident Management (Scheduled Phase 8)"; color: "#F59E0B" } }
            // 13: Admin Hazards Placeholder
            Rectangle { color: "#080C14"; Text { anchors.centerIn: parent; text: "Admin Hazard Containment Zones (Scheduled Phase 8)"; color: "#F59E0B" } }
            // 14: Admin Events Placeholder
            Rectangle { color: "#080C14"; Text { anchors.centerIn: parent; text: "Admin Emergency Events (Scheduled Phase 8)"; color: "#F59E0B" } }
            // 15: Admin Routes Placeholder
            Rectangle { color: "#080C14"; Text { anchors.centerIn: parent; text: "Admin Evacuation Corridor Inspector (Scheduled Phase 8)"; color: "#F59E0B" } }
            // 16: Admin Users Placeholder
            Rectangle { color: "#080C14"; Text { anchors.centerIn: parent; text: "Admin User Registry (Scheduled Phase 8)"; color: "#F59E0B" } }
            // 17: Admin Verification Placeholder
            Rectangle { color: "#080C14"; Text { anchors.centerIn: parent; text: "Admin Identity Verification Review (Scheduled Phase 8)"; color: "#F59E0B" } }
            // 18: Admin Alerts Placeholder
            Rectangle { color: "#080C14"; Text { anchors.centerIn: parent; text: "Admin Regional Alert Broadcast (Scheduled Phase 8)"; color: "#F59E0B" } }

            // 19: Full C++ Engine Prototype Lab (Preserved from Phase 1!)
            PrototypeLabView {}

        }
    }

    // -------------------------------------------------------------------------
    // Emergency Resolution Confirmation Dialog
    // Requires explicit confirmation before resolving active emergency
    // -------------------------------------------------------------------------
    Dialog {
        id: confirmResolveDialog
        title: qsTr("Confirm Emergency Resolution")
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 420)
        modal: true
        standardButtons: Dialog.Yes | Dialog.No

        background: Rectangle {
            color: "#0F172A"
            border.color: "#334155"
            border.width: 1
            radius: 12
        }

        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "Are you sure you want to resolve and complete this emergency?"
                color: "#F8FAFC"
                font.pixelSize: 13
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Text {
                text: "This will transition emergency record '" + emergencyService.activeEmergencyId +
                      "' to Completed in local SQLite storage. The record will remain archived in emergency history."
                color: "#94A3B8"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#1E293B"
            }
            Text {
                text: "⚠️ Hardware GPS, BLE Mesh broadcast, and Cloud sync were deferred per Phase 3 specifications."
                color: "#F59E0B"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        onAccepted: {
            appState.resolveSos();
        }
    }
}
