package com.example.georescux.domain.auth

import com.example.georescux.data.auth.UserRole
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.withTimeoutOrNull

/**
 * Encapsulates the administrative authorization gate to guarantee that
 * admin screens fail closed if unauthenticated, non-admin, or if an
 * error occurs during role verification.
 */
class AdminAuthGuard(
    private val roleResolver: RoleResolver,
    private val timeoutMs: Long = DEFAULT_TIMEOUT_MS
) {

    /**
     * Resolves the current user's role and returns true ONLY if the role is [UserRole.ADMIN].
     * Bounded by [timeoutMs]. Fails closed (returns false) on timeout, error, or non-admin role.
     * Rethrows [CancellationException] to preserve coroutine lifecycle cancellation.
     */
    suspend fun isAuthorizedAdmin(): Boolean = try {
        withTimeoutOrNull(timeoutMs) {
            roleResolver.resolveRole() == UserRole.ADMIN
        } == true
    } catch (e: CancellationException) {
        throw e
    } catch (_: Exception) {
        false
    }

    companion object {
        const val DEFAULT_TIMEOUT_MS = 3_000L
    }
}

