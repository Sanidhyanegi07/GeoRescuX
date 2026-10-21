package com.example.georescux.ui.contacts

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider
import com.example.georescux.GeoRescuXApplication
import com.example.georescux.R
import com.example.georescux.core.validation.ContactInputValidator
import com.example.georescux.domain.contacts.ContactPriority
import com.example.georescux.domain.contacts.EmergencyContact
import com.example.georescux.ui.common.ActiveSosBanner
import com.example.georescux.ui.common.BottomNav
import com.example.georescux.ui.common.HelpLauncher
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import java.util.UUID

/**
 * Emergency Contacts screen (reference 1, Screen 8):
 * - Tabs: My Contacts | Emergency Services
 * - Direct dialer integration for all emergency numbers and personal contacts
 * - Validated add contact dialog
 * - Confirmation on contact deletion
 * - Real local persistence via ContactsRepository
 */
class ContactsActivity : AppCompatActivity() {

    private val uiScope = CoroutineScope(Dispatchers.Main.immediate + SupervisorJob())
    private lateinit var viewModel: ContactsViewModel
    private var currentTabIsServices = false
    private var userContactsCache: List<EmergencyContact> = emptyList()

    override fun onResume() {
        super.onResume()
        ActiveSosBanner.refresh(this)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_contacts)

        findViewById<View>(R.id.buttonContactsBack)?.setOnClickListener { finish() }
        HelpLauncher.bind(this)
        BottomNav.bind(this, R.id.nav_contacts)

        val container = (application as GeoRescuXApplication).appContainer
        viewModel = ViewModelProvider(this, ContactsViewModel.Factory(container.contactsRepository))
            .get(ContactsViewModel::class.java)

        val tabMyContacts = findViewById<TextView>(R.id.tabMyContacts)
        val tabEmergencyServices = findViewById<TextView>(R.id.tabEmergencyServices)
        val buttonAdd = findViewById<Button>(R.id.buttonOpenAddContact)

        tabMyContacts?.setOnClickListener {
            currentTabIsServices = false
            tabMyContacts.setBackgroundResource(R.drawable.bg_tab_active)
            tabMyContacts.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.white))
            tabEmergencyServices?.background = null
            tabEmergencyServices?.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.text_muted))
            buttonAdd?.visibility = View.VISIBLE
            renderList()
        }

        tabEmergencyServices?.setOnClickListener {
            currentTabIsServices = true
            tabEmergencyServices.setBackgroundResource(R.drawable.bg_tab_active)
            tabEmergencyServices.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.white))
            tabMyContacts.background = null
            tabMyContacts.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.text_muted))
            buttonAdd?.visibility = View.GONE
            renderList()
        }

        buttonAdd?.setOnClickListener {
            showAddContactDialog()
        }

        uiScope.launch {
            viewModel.contacts.collect { contacts ->
                userContactsCache = contacts
                if (!currentTabIsServices) {
                    renderList()
                }
            }
        }
    }

    private fun renderList() {
        val container = findViewById<LinearLayout>(R.id.contactsListContainer) ?: return
        val emptyText = findViewById<View>(R.id.textViewEmptyContacts)
        container.removeAllViews()

        if (currentTabIsServices) {
            emptyText?.visibility = View.GONE
            val services = listOf(
                EmergencyContact("srv_112", "National Emergency Helpline", "112", "All Emergency Services", ContactPriority.PRIMARY),
                EmergencyContact("srv_108", "Disaster Medical & Ambulance", "108", "Medical Rescue", ContactPriority.PRIMARY),
                EmergencyContact("srv_1078", "Disaster Management Authority", "1078", "NDMA Helpline", ContactPriority.SECONDARY),
                EmergencyContact("srv_100", "Police Control Room", "100", "State Police", ContactPriority.SECONDARY),
                EmergencyContact("srv_101", "Fire & Rescue Department", "101", "Fire Services", ContactPriority.SECONDARY),
                EmergencyContact("srv_1091", "Women Safety Helpline", "1091", "National Commission", ContactPriority.SECONDARY),
            )
            services.forEach { service ->
                container.addView(contactRow(service, isService = true) { /* no-op */ })
            }
        } else {
            emptyText?.visibility = if (userContactsCache.isEmpty()) View.VISIBLE else View.GONE
            userContactsCache.forEach { contact ->
                container.addView(contactRow(contact, isService = false) { id ->
                    confirmDelete(contact)
                })
            }
        }
    }

    private fun contactRow(
        contact: EmergencyContact,
        isService: Boolean,
        onDelete: (String) -> Unit,
    ): View {
        val row = layoutInflater.inflate(R.layout.item_contact, findViewById<LinearLayout>(R.id.contactsListContainer), false)
        val nameView = row.findViewById<TextView>(R.id.textContactName)
        val priorityView = row.findViewById<TextView>(R.id.textContactPriority)
        val phoneView = row.findViewById<TextView>(R.id.textContactPhone)
        val relView = row.findViewById<TextView>(R.id.textContactRelationship)
        val initialsView = row.findViewById<TextView>(R.id.textContactInitials)
        val callBtn = row.findViewById<ImageButton>(R.id.buttonCallContact)
        val deleteBtn = row.findViewById<ImageButton>(R.id.buttonDeleteContact)

        nameView?.text = contact.name
        phoneView?.text = contact.phoneNumber
        relView?.text = contact.relationship

        val initials = contact.name.trim().take(1).uppercase()
        initialsView?.text = if (initials.isNotEmpty()) initials else "C"

        if (isService) {
            priorityView?.visibility = View.GONE
            deleteBtn?.visibility = View.GONE
        } else {
            if (contact.priority == ContactPriority.PRIMARY) {
                priorityView?.visibility = View.VISIBLE
                priorityView?.text = "Primary"
                priorityView?.setBackgroundResource(R.drawable.bg_pill_high_priority)
                priorityView?.setTextColor(androidx.core.content.ContextCompat.getColor(this, R.color.sos_red))
            } else {
                priorityView?.visibility = View.GONE
            }
            deleteBtn?.visibility = View.VISIBLE
            deleteBtn?.setOnClickListener { onDelete(contact.id) }
        }

        callBtn?.setOnClickListener {
            dialPhone(contact.phoneNumber)
        }

        return row
    }

    private fun dialPhone(phoneNumber: String) {
        val cleaned = phoneNumber.replace(" ", "").replace("-", "")
        try {
            val intent = Intent(Intent.ACTION_DIAL, Uri.parse("tel:$cleaned"))
            startActivity(intent)
        } catch (e: Exception) {
            Toast.makeText(this, "Could not open dialer for $phoneNumber", Toast.LENGTH_SHORT).show()
        }
    }

    private fun confirmDelete(contact: EmergencyContact) {
        MaterialAlertDialogBuilder(this)
            .setTitle("Remove Contact")
            .setMessage("Are you sure you want to remove ${contact.name} from your emergency contacts?")
            .setPositiveButton("Remove") { _, _ ->
                viewModel.deleteContact(contact.id)
                Toast.makeText(this, "${contact.name} removed", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showAddContactDialog() {
        val dialogView = LayoutInflater.from(this).inflate(R.layout.dialog_add_contact, null)
        val editName = dialogView.findViewById<EditText>(R.id.dialogEditContactName)
        val editPhone = dialogView.findViewById<EditText>(R.id.dialogEditContactPhone)
        val editRel = dialogView.findViewById<EditText>(R.id.dialogEditContactRelationship)
        val radioPrimary = dialogView.findViewById<RadioButton>(R.id.dialogRadioPrimary)
        val textError = dialogView.findViewById<TextView>(R.id.dialogTextContactError)
        val btnCancel = dialogView.findViewById<Button>(R.id.dialogButtonCancel)
        val btnSave = dialogView.findViewById<Button>(R.id.dialogButtonSave)

        val dialog = MaterialAlertDialogBuilder(this)
            .setView(dialogView)
            .setCancelable(true)
            .create()

        btnCancel.setOnClickListener { dialog.dismiss() }

        btnSave.setOnClickListener {
            val name = editName.text.toString().trim()
            val phone = editPhone.text.toString().trim()
            val rel = editRel.text.toString().trim()

            val err = ContactInputValidator.validateContact(name, phone, rel)
            if (err != null) {
                textError.text = err
                textError.visibility = View.VISIBLE
                return@setOnClickListener
            }

            textError.visibility = View.GONE
            val priority = if (radioPrimary.isChecked) ContactPriority.PRIMARY else ContactPriority.SECONDARY

            viewModel.addContact(
                EmergencyContact(
                    id = UUID.randomUUID().toString(),
                    name = name,
                    phoneNumber = phone,
                    relationship = rel,
                    priority = priority,
                )
            )

            Toast.makeText(this, "$name added to emergency contacts", Toast.LENGTH_SHORT).show()
            dialog.dismiss()
        }

        dialog.show()
    }

    override fun onDestroy() {
        uiScope.cancel()
        super.onDestroy()
    }
}
