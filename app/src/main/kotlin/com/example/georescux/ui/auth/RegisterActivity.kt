package com.example.georescux.ui.auth

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider
import com.example.georescux.GeoRescuXApplication
import com.example.georescux.R
import com.example.georescux.core.validation.AuthInputValidator
import com.example.georescux.ui.home.HomeActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

/**
 * Register screen. The UI validates the input, asks [AuthViewModel] to create
 * the account, and observes the result. Firebase is never touched here, and
 * passwords are stored by Firebase Authentication only.
 */
class RegisterActivity : AppCompatActivity() {

    private lateinit var authViewModel: AuthViewModel
    private var hasNavigated = false

    private val uiScope = CoroutineScope(Dispatchers.Main.immediate + SupervisorJob())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_register)

        val container = (application as GeoRescuXApplication).appContainer
        authViewModel = ViewModelProvider(this, AuthViewModel.Factory(container.authRepository, container.roleResolver))
            .get(AuthViewModel::class.java)

        val emailInput = findViewById<EditText>(R.id.editTextEmail)
        val passwordInput = findViewById<EditText>(R.id.editTextPassword)
        val confirmPasswordInput = findViewById<EditText>(R.id.editTextConfirmPassword)
        val errorText = findViewById<TextView>(R.id.textViewError)
        val registerButton = findViewById<Button>(R.id.buttonRegister)
        val backToLoginButton = findViewById<View>(R.id.buttonBackToLogin)
        val goToLoginButton = findViewById<View>(R.id.buttonGoToLogin)

        registerButton.setOnClickListener {
            if (authViewModel.uiState.value.isLoading) return@setOnClickListener

            val email = emailInput.text.toString().trim()
            val password = passwordInput.text.toString()
            val confirmPassword = confirmPasswordInput.text.toString()

            val validationError = AuthInputValidator.validateRegister(email, password, confirmPassword)
            if (validationError != null) {
                showError(errorText, validationError)
                return@setOnClickListener
            }

            authViewModel.register(email, password)
        }

        backToLoginButton.setOnClickListener {
            startActivity(Intent(this, LoginActivity::class.java))
            finish()
        }
        goToLoginButton?.setOnClickListener {
            startActivity(Intent(this, LoginActivity::class.java))
            finish()
        }

        observeAuthState(errorText, registerButton)
    }

    private fun observeAuthState(errorText: TextView, registerButton: Button) {
        val container = (application as GeoRescuXApplication).appContainer
        uiScope.launch {
            authViewModel.uiState.collect { state ->
                registerButton.isEnabled = !state.isLoading
                if (state.errorMessage != null) {
                    showError(errorText, state.errorMessage)
                } else {
                    errorText.visibility = View.GONE
                }
                if (state.isDone && !hasNavigated) {
                    hasNavigated = true
                    // Spec §6 & LoginActivity parity: a user without an explicitly selected
                    // region must be routed to RegionSelectionActivity. Returning users with
                    // a valid selected region go straight to HomeActivity.
                    val destination = resolvePostRegistrationDestination(container.hasSelectedRegion)
                    startActivity(Intent(this@RegisterActivity, destination))
                    finish()
                }
            }
        }
    }

    private fun showError(errorText: TextView, message: String) {
        errorText.text = message
        errorText.visibility = View.VISIBLE
    }

    override fun onDestroy() {
        uiScope.cancel()
        super.onDestroy()
    }

    companion object {
        fun resolvePostRegistrationDestination(hasSelectedRegion: Boolean): Class<*> =
            if (!hasSelectedRegion) {
                com.example.georescux.ui.region.RegionSelectionActivity::class.java
            } else {
                HomeActivity::class.java
            }
    }
}
