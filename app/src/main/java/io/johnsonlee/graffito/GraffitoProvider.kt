package io.johnsonlee.graffito

import android.content.*
import android.database.Cursor
import android.net.Uri
import android.os.IBinder
import android.util.Log

class GraffitoProvider : ContentProvider(), ServiceConnection {

    private companion object {
        const val TAG = "GraffitoProvider"

        init {
            System.loadLibrary("graffito")
        }
    }

    override fun onCreate(): Boolean = bindService()

    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?
    ): Cursor? = null

    override fun getType(uri: Uri): String? = null

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int = 0

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?
    ): Int = 0

    override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
        Log.i(TAG, "Service $name connected")
    }

    override fun onServiceDisconnected(name: ComponentName?) {
        Log.w(TAG, "Service $name disconnected")
    }

    private fun bindService(): Boolean {
        val service = Intent(context, TraceService::class.java)
        return context?.bindService(service, this, Context.BIND_AUTO_CREATE) == true
    }

}
