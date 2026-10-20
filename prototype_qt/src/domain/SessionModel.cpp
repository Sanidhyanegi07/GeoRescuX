#include "SessionModel.h"
#include <QDebug>

namespace georescux::domain {

SessionModel::SessionModel(QObject *parent)
    : QObject(parent)
{
}

QString SessionModel::sessionStateName() const noexcept {
    switch (m_sessionState) {
        case SessionState::Unauthenticated:  return QStringLiteral("Unauthenticated");
        case SessionState::SessionRestoring: return QStringLiteral("SessionRestoring");
        case SessionState::AuthenticatedUser: return QStringLiteral("AuthenticatedUser");
        case SessionState::AuthenticatedAdmin: return QStringLiteral("AuthenticatedAdmin");
        case SessionState::SessionError:     return QStringLiteral("SessionError");
    }
    return QStringLiteral("Unknown");
}

QString SessionModel::roleName() const noexcept {
    switch (m_currentUser.role) {
        case UserRole::None:            return QStringLiteral("None");
        case UserRole::CitizenUser:     return QStringLiteral("Citizen User");
        case UserRole::AuthorizedAdmin: return QStringLiteral("Authorized Administrator");
    }
    return QStringLiteral("Unknown");
}

bool SessionModel::isAuthenticated() const noexcept {
    return (m_sessionState == SessionState::AuthenticatedUser && m_currentUser.role == UserRole::CitizenUser) ||
           (m_sessionState == SessionState::AuthenticatedAdmin && m_currentUser.role == UserRole::AuthorizedAdmin);
}

bool SessionModel::isAdmin() const noexcept {
    return m_sessionState == SessionState::AuthenticatedAdmin &&
           m_currentUser.role == UserRole::AuthorizedAdmin;
}

bool SessionModel::loginDevelopmentUser(
    const QString &displayName,
    const QString &email,
    const QString &phone
) {
    qWarning().noquote() << "[SessionModel] [DEV TEST FACILITY] loginDevelopmentUser invoked for:"
                         << displayName << "<" << email << "> (NOT a production authentication bypass)";

    if (displayName.trimmed().isEmpty() || email.trimmed().isEmpty()) {
        setSessionError(QStringLiteral("Display name and email must not be empty."));
        return false;
    }

    UserProfile profile;
    profile.uid = QStringLiteral("dev_user_") + QString::number(QDateTime::currentMSecsSinceEpoch());
    profile.displayName = displayName.trimmed();
    profile.email = email.trimmed();
    profile.phone = phone.trimmed();
    profile.authProvider = QStringLiteral("development_fixture");
    profile.role = UserRole::CitizenUser;
    profile.isVerified = true;
    profile.lastLoginMs = QDateTime::currentMSecsSinceEpoch();

    m_lastError.clear();
    updateState(SessionState::AuthenticatedUser, profile);
    return true;
}

bool SessionModel::loginDevelopmentAdmin(
    const QString &displayName,
    const QString &email
) {
    qWarning().noquote() << "[SessionModel] [DEV TEST FACILITY] loginDevelopmentAdmin invoked for:"
                         << displayName << "<" << email << "> (NOT a production authentication bypass)";

    if (displayName.trimmed().isEmpty() || email.trimmed().isEmpty()) {
        setSessionError(QStringLiteral("Administrator name and email must not be empty."));
        return false;
    }

    UserProfile profile;
    profile.uid = QStringLiteral("dev_admin_") + QString::number(QDateTime::currentMSecsSinceEpoch());
    profile.displayName = displayName.trimmed();
    profile.email = email.trimmed();
    profile.phone = QStringLiteral("+91-0000000000");
    profile.authProvider = QStringLiteral("development_fixture");
    profile.role = UserRole::AuthorizedAdmin;
    profile.isVerified = true;
    profile.lastLoginMs = QDateTime::currentMSecsSinceEpoch();

    m_lastError.clear();
    updateState(SessionState::AuthenticatedAdmin, profile);
    return true;
}

bool SessionModel::restoreSession(const UserProfile &profile) {
    if (!profile.isValid()) {
        setSessionError(QStringLiteral("Cannot restore invalid session profile."));
        return false;
    }

    // Security Rule: Cached role metadata is NEVER proof of Admin authorization.
    // Administrative privileges require explicit re-authentication.
    if (profile.role == UserRole::AuthorizedAdmin) {
        qWarning().noquote() << "[SessionModel] Security Policy Violation: Refusing to restore Admin session from unauthenticated local cache for UID:" << profile.uid;
        setSessionError(QStringLiteral("Administrative sessions cannot be restored from local cache; explicit re-authentication required."));
        return false;
    }

    // For Citizen User: restore cached profile
    m_lastError.clear();
    UserProfile restoredProfile = profile;
    if (restoredProfile.authProvider.isEmpty()) {
        restoredProfile.authProvider = QStringLiteral("local_cache");
    }

    updateState(SessionState::AuthenticatedUser, restoredProfile);
    qInfo().noquote() << "[SessionModel] Restored Citizen User session from local cache for:" << restoredProfile.displayName;
    return true;
}

void SessionModel::logout() {
    m_lastError.clear();
    UserProfile emptyProfile;
    updateState(SessionState::Unauthenticated, emptyProfile);
    qInfo() << "[SessionModel] User session cleanly logged out. State reset to Unauthenticated.";
}

void SessionModel::setSessionError(const QString &errorMessage) {
    m_lastError = errorMessage;
    emit errorOccurred(m_lastError);
    if (!isAuthenticated()) {
        UserProfile emptyProfile;
        updateState(SessionState::SessionError, emptyProfile);
    }
}

void SessionModel::updateState(SessionState newState, const UserProfile &newProfile) {
    // Invariant Enforcement: Validate (newState, newProfile.role) consistency
    UserProfile sanitizedProfile = newProfile;

    switch (newState) {
        case SessionState::Unauthenticated:
        case SessionState::SessionError:
            // An unauthenticated or error state MUST NOT retain a role or active identity
            sanitizedProfile = UserProfile{};
            break;

        case SessionState::AuthenticatedUser:
            if (sanitizedProfile.role != UserRole::CitizenUser || !sanitizedProfile.isValid()) {
                qCritical().noquote() << "[SessionModel] Invariant violation: AuthenticatedUser requires valid CitizenUser role. Transition aborted.";
                m_lastError = QStringLiteral("Invalid session state transition: role mismatch for AuthenticatedUser.");
                emit errorOccurred(m_lastError);
                m_sessionState = SessionState::SessionError;
                m_currentUser = UserProfile{};
                emit sessionStateChanged(m_sessionState);
                emit userRoleChanged(m_currentUser.role);
                emit currentUserChanged();
                return;
            }
            break;

        case SessionState::AuthenticatedAdmin:
            if (sanitizedProfile.role != UserRole::AuthorizedAdmin || !sanitizedProfile.isValid()) {
                qCritical().noquote() << "[SessionModel] Invariant violation: AuthenticatedAdmin requires valid AuthorizedAdmin role. Transition aborted.";
                m_lastError = QStringLiteral("Invalid session state transition: role mismatch for AuthenticatedAdmin.");
                emit errorOccurred(m_lastError);
                m_sessionState = SessionState::SessionError;
                m_currentUser = UserProfile{};
                emit sessionStateChanged(m_sessionState);
                emit userRoleChanged(m_currentUser.role);
                emit currentUserChanged();
                return;
            }
            break;

        case SessionState::SessionRestoring:
            // Restoring is an intermediate transient state without active user privileges
            sanitizedProfile = UserProfile{};
            break;
    }

    const bool stateChanged = (m_sessionState != newState);
    const bool roleChanged = (m_currentUser.role != sanitizedProfile.role);
    const bool userChanged = (m_currentUser.uid != sanitizedProfile.uid ||
                              m_currentUser.displayName != sanitizedProfile.displayName ||
                              m_currentUser.email != sanitizedProfile.email);

    m_sessionState = newState;
    m_currentUser = sanitizedProfile;

    if (stateChanged) {
        emit sessionStateChanged(m_sessionState);
    }
    if (roleChanged) {
        emit userRoleChanged(m_currentUser.role);
    }
    if (userChanged) {
        emit currentUserChanged();
    }
}

} // namespace georescux::domain
