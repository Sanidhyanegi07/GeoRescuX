package com.example.georescux.domain.auth

import com.example.georescux.data.auth.UserRole

/**
 * Encapsulates the administrative authorization gate to guarantee that
 * admin screens fail closed if unauthenticated, non-admin, or if an
 * error occurs during role verification.
 */
class AdminAuthGuard(private val roleResolver: RoleResolver) {

    /**
     * Resolves the current user's role and returns true ONLY if the role is [UserRole.ADMIN].
     * Fails closed (returns false) on any exception or non-admin role.
     */
    suspend fun isAuthorizedAdmin(): Boolean = try {
        roleResolver.resolveRole() == UserRole.ADMIN
    } catch (_: Exception) {
        false
    }
}

