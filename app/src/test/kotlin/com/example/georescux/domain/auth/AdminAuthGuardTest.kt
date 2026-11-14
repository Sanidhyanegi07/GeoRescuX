package com.example.georescux.domain.auth

import com.example.georescux.data.auth.UserRole
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AdminAuthGuardTest {

    private class FakeRoleResolver(
        private val role: UserRole? = null,
        private val shouldThrow: Boolean = false,
    ) : RoleResolver {
        override suspend fun resolveRole(): UserRole {
            if (shouldThrow) throw IllegalStateException("Network or Firebase auth error")
            return role ?: UserRole.UNKNOWN
        }
    }

    @Test
    fun `isAuthorizedAdmin returns true when role is ADMIN`() = runBlocking {
        val guard = AdminAuthGuard(FakeRoleResolver(UserRole.ADMIN))
        assertTrue(guard.isAuthorizedAdmin())
    }

    @Test
    fun `isAuthorizedAdmin returns false when role is USER`() = runBlocking {
        val guard = AdminAuthGuard(FakeRoleResolver(UserRole.USER))
        assertFalse(guard.isAuthorizedAdmin())
    }

    @Test
    fun `isAuthorizedAdmin returns false when role is UNKNOWN`() = runBlocking {
        val guard = AdminAuthGuard(FakeRoleResolver(UserRole.UNKNOWN))
        assertFalse(guard.isAuthorizedAdmin())
    }

    @Test
    fun `isAuthorizedAdmin fails closed and returns false on exception`() = runBlocking {
        val guard = AdminAuthGuard(FakeRoleResolver(shouldThrow = true))
        assertFalse(guard.isAuthorizedAdmin())
    }
}

