package com.example.georescux

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.example.georescux.ui.admin.AdminDashboardActivity
import com.example.georescux.ui.auth.LoginActivity
import com.example.georescux.ui.home.HomeActivity
import com.example.georescux.ui.main.RoleRoutingCoordinator
import com.example.georescux.ui.main.RoleRoutingState
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Entry point of the app (declared as the launcher activity in the manifest).
 * Displays a branded loading screen immediately and coordinates role-based routing
 * asynchronously with bounded timeout and fail-closed safety.
 */
class MainActivity : AppCompatActivity() {

    private val hasNavigated = AtomicBoolean(false)
    private lateinit var coordinator: RoleRoutingCoordinator

    private lateinit var progressRoleResolution: ProgressBar
    private lateinit var textRoleStatus: TextView
    private lateinit var layoutRoleRecovery: LinearLayout
    private lateinit var textRoleError: TextView
    private lateinit var buttonContinueStandard: Button
    private lateinit var buttonRetryRole: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // 1. Immediately render window and layout to prevent window-focus ANR
        setContentView(R.layout.activity_main)

        progressRoleResolution = findViewById(R.id.progressRoleResolution)
        textRoleStatus = findViewById(R.id.textRoleStatus)
        layoutRoleRecovery = findViewById(R.id.layoutRoleRecovery)
        textRoleError = findViewById(R.id.textRoleError)
        buttonContinueStandard = findViewById(R.id.buttonContinueStandard)
        buttonRetryRole = findViewById(R.id.buttonRetryRole)

        val appContainer = (application as GeoRescuXApplication).appContainer
        coordinator = RoleRoutingCoordinator(appContainer.roleResolver)

        buttonContinueStandard.setOnClickListener {
            coordinator.continueStandard()
        }

        buttonRetryRole.setOnClickListener {
            coordinator.retry(lifecycleScope)
        }

        lifecycleScope.launch {
            coordinator.state.collectLatest { state ->
                renderState(state)
            }
        }

        coordinator.start(lifecycleScope, appContainer.authRepository.isSignedIn)
    }

    private fun renderState(state: RoleRoutingState) {
        if (isFinishing || isDestroyed) return
        when (state) {
            RoleRoutingState.Idle -> {
                progressRoleResolution.visibility = View.VISIBLE
                textRoleStatus.visibility = View.VISIBLE
                textRoleStatus.text = getString(R.string.role_verifying)
                layoutRoleRecovery.visibility = View.GONE
            }
            is RoleRoutingState.Loading -> {
                progressRoleResolution.visibility = View.VISIBLE
                textRoleStatus.visibility = View.VISIBLE
                textRoleStatus.text = state.message
                layoutRoleRecovery.visibility = View.GONE
            }
            is RoleRoutingState.ResolutionFailed -> {
                progressRoleResolution.visibility = View.GONE
                textRoleStatus.visibility = View.GONE
                layoutRoleRecovery.visibility = View.VISIBLE
                textRoleError.text = state.message
            }
            RoleRoutingState.NavigateToLogin -> {
                navigateTo(LoginActivity::class.java)
            }
            RoleRoutingState.NavigateToHome -> {
                navigateTo(HomeActivity::class.java)
            }
            RoleRoutingState.NavigateToAdmin -> {
                navigateTo(AdminDashboardActivity::class.java)
            }
        }
    }

    private fun navigateTo(destination: Class<*>) {
        if (!hasNavigated.compareAndSet(false, true)) return
        startActivity(Intent(this, destination))
        finish()
    }
}
