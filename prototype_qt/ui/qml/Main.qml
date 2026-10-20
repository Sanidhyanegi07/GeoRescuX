import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

            // 2: User Home
            Rectangle {
                color: "#080C14"
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
                                    Text { text: "Schema V1 Active"; color: "#38BDF8"; font.pixelSize: 10 }
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

            // 3: User Map Placeholder
            Rectangle {
                color: "#080C14"
                ColumnLayout {
                    anchors.centerIn: parent; width: Math.min(parent.width - 40, 420); spacing: 10
                    Text { text: "🗺️ Offline Regional Map (Phase 5 Placeholder)"; color: "#F8FAFC"; font.pixelSize: 16; font.weight: Font.Bold }
                    Text { text: "Status: Mapsforge binary vector map renderer integration is scheduled for Phase 5. Bundled vector dataset: 'uttarakhand-state.map' (26.4 MB)."; color: "#94A3B8"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }

            // 4: User Safe Route
            Rectangle {
                color: "#080C14"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 12
                    Text { text: "🧭 Offline Safe Evacuation Route"; color: "#F8FAFC"; font.pixelSize: 16; font.weight: Font.Bold }
                    Text { text: "Computes evacuation corridors using local C++20 A* pathfinding and 3D elevation impedance models."; color: "#94A3B8"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    Button {
                        Layout.fillWidth: true; Layout.preferredHeight: 42
                        background: Rectangle { radius: 8; color: "#0284C7" }
                        contentItem: Text { text: "▶  Compute Safe Route via C++ Engine"; color: "#FFFFFF"; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: prototypeBackend.runAStarRoutingBenchmark(true)
                    }

                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true; radius: 10; color: "#0F172A"; border.color: "#1E293B"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 6
                            Text { text: "ROUTING RESULT:"; color: "#64748B"; font.pixelSize: 10; font.weight: Font.Bold }
                            Text { text: prototypeBackend.routeSummary; color: "#34D399"; font.pixelSize: 11; font.family: "monospace"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                    }
                }
            }

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
                                { q: "How does GeoRescuX work without cellular networks?", a: "GeoRescuX uses bundled offline topological road graphs and an autonomous peer-to-peer BLE mesh to communicate directly between nearby phones." },
                                { q: "What is Safe Route?", a: "Safe Route calculates terrain-impedance and hazard-avoidance evacuation paths using local C++20 A* pathfinding." },
                                { q: "How does the SOS activation countdown work?", a: "SOS requires a continuous 2-second hold to activate, preventing accidental triggers while enabling rapid deployment." },
                                { q: "Is my personal data encrypted?", a: "Yes. All local state is persisted via structured SQLite tables, and BLE packets are signed using HMAC-SHA256." }
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 80; radius: 8; color: "#0F172A"; border.color: "#1E293B"
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 10; spacing: 4
                                    Text { text: modelData.q; color: "#38BDF8"; font.pixelSize: 11; font.weight: Font.Bold }
                                    Text { text: modelData.a; color: "#CBD5E1"; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
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
            Rectangle {
                color: "#080C14"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Text { text: "🔬 C++20 Search & Rescue Engine Lab (Phase 1 Baseline)"; color: "#38BDF8"; font.pixelSize: 14; font.weight: Font.Bold }

                    // Visual 2D Map Canvas
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 180; radius: 10; color: "#0D131F"; border.color: "#1E293B"; clip: true
                        Canvas {
                            id: labCanvas
                            anchors.fill: parent; anchors.margins: 6
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                var w = width; var h = height;
                                ctx.strokeStyle = "#38BDF8"; ctx.lineWidth = 3;
                                ctx.beginPath(); ctx.moveTo(w * 0.1, h * 0.8); ctx.lineTo(w * 0.3, h * 0.6);
                                ctx.lineTo(w * 0.5, h * 0.7); ctx.lineTo(w * 0.8, h * 0.3); ctx.stroke();
                                ctx.fillStyle = "#EF4444"; ctx.beginPath(); ctx.arc(w * 0.5, h * 0.3, 18, 0, 2*Math.PI); ctx.fill();
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Button {
                            Layout.fillWidth: true; Layout.preferredHeight: 38
                            background: Rectangle { radius: 6; color: "#0284C7" }
                            contentItem: Text { text: "A* Pathfinding"; color: "#FFFFFF"; font.pixelSize: 11; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: prototypeBackend.runAStarRoutingBenchmark(true)
                        }
                        Button {
                            Layout.fillWidth: true; Layout.preferredHeight: 38
                            background: Rectangle { radius: 6; color: "#EA580C" }
                            contentItem: Text { text: "BLE Mesh Packet"; color: "#FFFFFF"; font.pixelSize: 11; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: prototypeBackend.runMeshPacketBenchmark(3, 85, 4)
                        }
                        Button {
                            Layout.fillWidth: true; Layout.preferredHeight: 38
                            background: Rectangle { radius: 6; color: "#DC2626" }
                            contentItem: Text { text: "START Triage"; color: "#FFFFFF"; font.pixelSize: 11; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: prototypeBackend.runTriageAssessment(false, true, 28, true, true, true)
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true; radius: 8; color: "#0B0F19"; border.color: "#1E293B"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 4
                            Text { text: "ENGINE STATUS FEED:"; color: "#64748B"; font.pixelSize: 9; font.weight: Font.Bold }
                            Text { text: prototypeBackend.statusText; color: "#34D399"; font.pixelSize: 11; font.family: "monospace"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            Rectangle { Layout.fillWidth: true; height: 1; color: "#1F2937" }
                            Text { text: "ROUTE SUMMARY:"; color: "#64748B"; font.pixelSize: 9; font.weight: Font.Bold }
                            Text { text: prototypeBackend.routeSummary; color: "#38BDF8"; font.pixelSize: 10; font.family: "monospace"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }
}
