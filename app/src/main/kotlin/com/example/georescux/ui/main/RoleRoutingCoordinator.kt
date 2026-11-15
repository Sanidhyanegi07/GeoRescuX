package com.example.georescux.ui.main

import com.example.georescux.data.auth.UserRole
import com.example.georescux.domain.auth.RoleResolver
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import java.util.concurrent.atomic.AtomicBoolean

sealed interface RoleRoutingState {
    object Idle : RoleRoutingState
    data class Loading(val message: String = "Verifying authorization...") : RoleRoutingState
    object NavigateToLogin : RoleRoutingState
    object NavigateToHome : RoleRoutingState
    object NavigateToAdmin : RoleRoutingState
    data class ResolutionFailed(val isTimeout: Boolean, val message: String) : RoleRoutingState
}

/**
 * Coordinates role-based routing from MainActivity with bounded timeout,
 * asynchronous non-blocking resolution, strict fail-closed authorization,
 * cancellation propagation, and duplicate navigation prevention.
 */
class RoleRoutingCoordinator(
    private val roleResolver: RoleResolver,
    val timeoutMs: Long = DEFAULT_TIMEOUT_MS
) {
    companion object {
        const val DEFAULT_TIMEOUT_MS = 3000L
    }

    private val _state = MutableStateFlow<RoleRoutingState>(RoleRoutingState.Idle)
    val state: StateFlow<RoleRoutingState> = _state.asStateFlow()

    private val _hasNavigated = AtomicBoolean(false)
    val hasNavigated: Boolean
        get() = _hasNavigated.get()

    private var activeJob: Job? = null

    fun start(scope: CoroutineScope, isSignedIn: Boolean) {
        if (_hasNavigated.get()) return
        if (!isSignedIn) {
            triggerNavigation(RoleRoutingState.NavigateToLogin)
            return
        }
        executeResolution(scope, forceRefresh = false)
    }

    fun retry(scope: CoroutineScope) {
        if (_hasNavigated.get()) return
        activeJob?.cancel()
        executeResolution(scope, forceRefresh = true)
    }

    fun continueStandard() {
        if (_hasNavigated.get()) return
        activeJob?.cancel()
        triggerNavigation(RoleRoutingState.NavigateToHome)
    }

    private fun executeResolution(scope: CoroutineScope, forceRefresh: Boolean) {
        _state.value = RoleRoutingState.Loading(
            if (forceRefresh) "Retrying authorization..." else "Verifying authorization..."
        )
        activeJob = scope.launch {
            try {
                val role = withTimeout(timeoutMs) {
                    roleResolver.resolveRole(forceRefresh)
                }
                when (role) {
                    UserRole.ADMIN -> triggerNavigation(RoleRoutingState.NavigateToAdmin)
                    UserRole.USER -> triggerNavigation(RoleRoutingState.NavigateToHome)
                    UserRole.UNKNOWN -> {
                        _state.value = RoleRoutingState.ResolutionFailed(
                            isTimeout = false,
                            message = "Could not verify administrative privileges. You may continue to the standard dashboard or retry."
                        )
                    }
                }
            } catch (e: TimeoutCancellationException) {
                _state.value = RoleRoutingState.ResolutionFailed(
                    isTimeout = true,
                    message = "Authorization verification timed out after ${timeoutMs}ms. You may continue to the standard dashboard or retry."
                )
            } catch (e: CancellationException) {
                // Must never swallow CancellationException: rethrow to respect structured concurrency
                throw e
            } catch (t: Throwable) {
                _state.value = RoleRoutingState.ResolutionFailed(
                    isTimeout = false,
                    message = t.message ?: "Authentication error occurred during role verification."
                )
            }
        }
    }

    private fun triggerNavigation(destination: RoleRoutingState) {
        if (_hasNavigated.compareAndSet(false, true)) {
            _state.value = destination
        }
    }
}
