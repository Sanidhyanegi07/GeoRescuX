package com.example.georescux.ui.main

import com.example.georescux.data.auth.UserRole
import com.example.georescux.domain.auth.RoleResolver
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class RoleRoutingCoordinatorTest {

    private class FakeRoleResolver(
        var roleToReturn: UserRole = UserRole.USER,
        var delayMs: Long = 0L,
        var exceptionToThrow: Throwable? = null
    ) : RoleResolver {
        var callCount: Int = 0
        var lastForceRefresh: Boolean? = null

        override suspend fun resolveRole(): UserRole = resolveRole(false)

        override suspend fun resolveRole(forceRefresh: Boolean): UserRole {
            callCount++
            lastForceRefresh = forceRefresh
            if (delayMs > 0) delay(delayMs)
            exceptionToThrow?.let { throw it }
            return roleToReturn
        }
    }

    @Test
    fun `1 successful USER resolution routes to Home`() = runTest {
        val resolver = FakeRoleResolver(roleToReturn = UserRole.USER)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = true)
        advanceUntilIdle()

        assertEquals(RoleRoutingState.NavigateToHome, coordinator.state.value)
        assertTrue(coordinator.hasNavigated)
    }

    @Test
    fun `2 successful ADMIN resolution routes to AdminDashboard`() = runTest {
        val resolver = FakeRoleResolver(roleToReturn = UserRole.ADMIN)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = true)
        advanceUntilIdle()

        assertEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)
        assertTrue(coordinator.hasNavigated)
    }

    @Test
    fun `3 missing admin claim routes to Home`() = runTest {
        // When user has valid token but claims don't include admin (normal user)
        val resolver = FakeRoleResolver(roleToReturn = UserRole.USER)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = true)
        advanceUntilIdle()

        assertEquals(RoleRoutingState.NavigateToHome, coordinator.state.value)
        assertNotEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)
        assertTrue(coordinator.hasNavigated)
    }

    @Test
    fun `4 token retrieval failure fails closed to UNKNOWN failed state`() = runTest {
        val resolver = FakeRoleResolver(roleToReturn = UserRole.UNKNOWN)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = true)
        advanceUntilIdle()

        val state = coordinator.state.value
        assertTrue("State should be ResolutionFailed", state is RoleRoutingState.ResolutionFailed)
        val failed = state as RoleRoutingState.ResolutionFailed
        assertFalse("Should not be a timeout failure", failed.isTimeout)
        assertFalse("Should not have navigated", coordinator.hasNavigated)
    }

    @Test
    fun `4b exception during token retrieval fails closed securely`() = runTest {
        val resolver = FakeRoleResolver(exceptionToThrow = RuntimeException("Firebase network error"))
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = true)
        advanceUntilIdle()

        val state = coordinator.state.value
        assertTrue("State should be ResolutionFailed", state is RoleRoutingState.ResolutionFailed)
        val failed = state as RoleRoutingState.ResolutionFailed
        assertFalse(failed.isTimeout)
        assertEquals("Firebase network error", failed.message)
        assertFalse("Should not have navigated", coordinator.hasNavigated)
    }

    @Test
    fun `5 timeout fails closed to timeout failure state`() = runTest {
        // Resolver takes 5000ms which exceeds the 3000ms timeout
        val resolver = FakeRoleResolver(roleToReturn = UserRole.ADMIN, delayMs = 5000L)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = true)
        advanceTimeBy(3001L)

        val state = coordinator.state.value
        assertTrue("State should be ResolutionFailed", state is RoleRoutingState.ResolutionFailed)
        val failed = state as RoleRoutingState.ResolutionFailed
        assertTrue("isTimeout should be true", failed.isTimeout)
        assertFalse("Should not have navigated to admin", coordinator.hasNavigated)
        assertNotEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)
    }

    @Test
    fun `6 coroutine cancellation rethrows CancellationException`() = runTest {
        val resolver = FakeRoleResolver(roleToReturn = UserRole.ADMIN, delayMs = 5000L)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        val job = launch {
            coordinator.start(this, isSignedIn = true)
        }
        advanceTimeBy(500L)
        assertTrue("State should be Loading while in flight", coordinator.state.value is RoleRoutingState.Loading)
        job.cancel()
        advanceUntilIdle()

        // If CancellationException was swallowed by catch(Throwable), state would be ResolutionFailed
        assertFalse("CancellationException must be rethrown and not swallowed into ResolutionFailed", coordinator.state.value is RoleRoutingState.ResolutionFailed)
        assertFalse("Must not have navigated", coordinator.hasNavigated)
    }

    @Test
    fun `7 retry after connectivity returns succeeds and routes to Admin`() = runTest {
        val resolver = FakeRoleResolver(roleToReturn = UserRole.UNKNOWN)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = true)
        advanceUntilIdle()

        assertTrue(coordinator.state.value is RoleRoutingState.ResolutionFailed)
        assertFalse(coordinator.hasNavigated)

        // Connectivity returns; user is now identified as ADMIN
        resolver.roleToReturn = UserRole.ADMIN
        coordinator.retry(this)
        advanceUntilIdle()

        assertEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)
        assertTrue(coordinator.hasNavigated)
        assertEquals(true, resolver.lastForceRefresh)
    }

    @Test
    fun `8 UNKNOWN state cannot access AdminDashboard`() = runTest {
        val resolver = FakeRoleResolver(roleToReturn = UserRole.UNKNOWN)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = true)
        advanceUntilIdle()

        assertTrue(coordinator.state.value is RoleRoutingState.ResolutionFailed)
        assertNotEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)
        assertFalse(coordinator.hasNavigated)

        // User falls back to standard user dashboard
        coordinator.continueStandard()
        assertEquals(RoleRoutingState.NavigateToHome, coordinator.state.value)
        assertNotEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)
        assertTrue(coordinator.hasNavigated)
    }

    @Test
    fun `9 duplicate navigation is prevented idempotent hasNavigated`() = runTest {
        val resolver = FakeRoleResolver(roleToReturn = UserRole.ADMIN)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = true)
        advanceUntilIdle()

        assertEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)
        assertTrue(coordinator.hasNavigated)
        val callCountAfterFirstNav = resolver.callCount

        // Attempting duplicate navigation via continueStandard()
        coordinator.continueStandard()
        assertEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)

        // Attempting retry() after navigation
        coordinator.retry(this)
        advanceUntilIdle()
        assertEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)
        assertEquals(callCountAfterFirstNav, resolver.callCount)

        // Attempting start() again
        coordinator.start(this, isSignedIn = true)
        advanceUntilIdle()
        assertEquals(RoleRoutingState.NavigateToAdmin, coordinator.state.value)
        assertEquals(callCountAfterFirstNav, resolver.callCount)
    }

    @Test
    fun `unauthenticated user routes immediately to Login`() = runTest {
        val resolver = FakeRoleResolver(roleToReturn = UserRole.ADMIN)
        val coordinator = RoleRoutingCoordinator(resolver, timeoutMs = 3000L)

        coordinator.start(this, isSignedIn = false)
        advanceUntilIdle()

        assertEquals(RoleRoutingState.NavigateToLogin, coordinator.state.value)
        assertTrue(coordinator.hasNavigated)
        assertEquals(0, resolver.callCount)
    }
}
