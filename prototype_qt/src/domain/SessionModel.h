#ifndef GEORESCUX_SESSION_MODEL_H
#define GEORESCUX_SESSION_MODEL_H

#include <QObject>
#include <QString>
#include <QDateTime>

namespace georescux::domain {

/**
 * @brief Authoritative lifecycle state of the user session.
 */
enum class SessionState {
    Unauthenticated = 0,
    SessionRestoring = 1,
    AuthenticatedUser = 2,
    AuthenticatedAdmin = 3,
    SessionError = 4
};

/**
 * @brief Authorized role governing portal navigation and access permissions.
 */
enum class UserRole {
    None = 0,
    CitizenUser = 1,
    AuthorizedAdmin = 2
};

/**
 * @brief Immutable profile snapshot of the actively authenticated entity.
 */
struct UserProfile {
    QString uid;
    QString displayName;
    QString email;
    QString phone;
    QString authProvider; // e.g. "development_fixture", "local_cache", "firebase"
    UserRole role{UserRole::None};
    bool isVerified{false};
    qint64 lastLoginMs{0};

    [[nodiscard]] bool isValid() const noexcept {
        return !uid.trimmed().isEmpty() && !displayName.trimmed().isEmpty() && role != UserRole::None;
    }
};

/**
 * @brief Centralized C++ Session Controller managing authentication lifecycle and roles.
 *
 * Guarantees that authentication state, user identity, and authorization role
 * are strictly typed, encapsulated, and decoupled from UI views.
 */
class SessionModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(SessionState sessionState READ sessionState NOTIFY sessionStateChanged)
    Q_PROPERTY(UserRole currentRole READ currentRole NOTIFY userRoleChanged)
    Q_PROPERTY(QString sessionStateName READ sessionStateName NOTIFY sessionStateChanged)
    Q_PROPERTY(QString roleName READ roleName NOTIFY userRoleChanged)
    Q_PROPERTY(QString currentUserId READ currentUserId NOTIFY currentUserChanged)
    Q_PROPERTY(QString currentUserName READ currentUserName NOTIFY currentUserChanged)
    Q_PROPERTY(QString currentUserEmail READ currentUserEmail NOTIFY currentUserChanged)
    Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY sessionStateChanged)
    Q_PROPERTY(bool isAdmin READ isAdmin NOTIFY userRoleChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

public:
    explicit SessionModel(QObject *parent = nullptr);
    ~SessionModel() override = default;

    [[nodiscard]] SessionState sessionState() const noexcept { return m_sessionState; }
    [[nodiscard]] UserRole currentRole() const noexcept { return m_currentUser.role; }
    [[nodiscard]] QString sessionStateName() const noexcept;
    [[nodiscard]] QString roleName() const noexcept;
    [[nodiscard]] const UserProfile &currentUser() const noexcept { return m_currentUser; }
    [[nodiscard]] QString currentUserId() const noexcept { return m_currentUser.uid; }
    [[nodiscard]] QString currentUserName() const noexcept { return m_currentUser.displayName; }
    [[nodiscard]] QString currentUserEmail() const noexcept { return m_currentUser.email; }
    [[nodiscard]] bool isAuthenticated() const noexcept;
    [[nodiscard]] bool isAdmin() const noexcept;
    [[nodiscard]] QString lastError() const noexcept { return m_lastError; }

    /**
     * @brief Visibly labeled test facility for Citizen User role switching.
     * Note: strictly for development and testing of navigation workflows.
     */
    Q_INVOKABLE bool loginDevelopmentUser(
        const QString &displayName,
        const QString &email,
        const QString &phone = QStringLiteral("+91-9876543210")
    );

    /**
     * @brief Visibly labeled test facility for Authorized Administrator role switching.
     * Note: strictly for development and testing of navigation workflows.
     */
    Q_INVOKABLE bool loginDevelopmentAdmin(
        const QString &displayName,
        const QString &email
    );

    /**
     * @brief Restores a previously verified and persisted local session.
     */
    bool restoreSession(const UserProfile &profile);

    /**
     * @brief Terminates the active session and resets state to Unauthenticated.
     */
    Q_INVOKABLE void logout();

    /**
     * @brief Records a session error and transitions to SessionError state.
     */
    void setSessionError(const QString &errorMessage);

signals:
    void sessionStateChanged(SessionState newState);
    void userRoleChanged(UserRole newRole);
    void currentUserChanged();
    void errorOccurred(const QString &errorMessage);

private:
    void updateState(SessionState newState, const UserProfile &newProfile);

    SessionState m_sessionState{SessionState::Unauthenticated};
    UserProfile m_currentUser;
    QString m_lastError;
};

} // namespace georescux::domain

#endif // GEORESCUX_SESSION_MODEL_H
