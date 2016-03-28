using UnityEditor;
using UnityEngine;
using System;
using System.Runtime.InteropServices;
using TestMySpline;


public class SpecDelayCustomGUI:IAudioEffectPluginGUI
{
	private int sampleRate;
	float windowSize = 0;
	private int brushSize = 7;
	private Color binCol = new Color (0.41f, 0.37f, 0.47f, 0.7f);
	private Color tickCol = new Color (1.0f, 1.0f, 1.0f, 0.5f);
	private Color specCol = new Color (0.63f, 0.54f, 0.51f, 0.9f);

	public override string Name {
		get { return "Spectral Delay"; }
	}

	public override string Description {
		get { return "Spectral delay plugin for Unity's audio plugin system"; }
	}
		
	public override string Vendor {
		get { return "ABA - AlexBaldwinAudio - 2016"; }
	}

	public override bool OnGUI (IAudioEffectPlugin plugin)
	{
		bool winChange = false;
		bool rectChange = false;
		float tempWin;
		float dispSpecFloat; 
		float maxDelFloat;
		// Get parameters from plugin
		plugin.GetFloatParameter ("Show Spectrum", out dispSpecFloat);
		plugin.SetFloatParameter ("Show Spectrum", Mathf.Round (dispSpecFloat));
		plugin.GetFloatParameter ("FFT Window Size", out tempWin);
		plugin.GetFloatParameter ("Maximum Delay", out maxDelFloat);
		plugin.SetFloatParameter ("Maximum Delay", Mathf.Round (maxDelFloat));
		sampleRate = plugin.GetSampleRate ();
		// Get the GUI Array from CPP plugin
		float[] guiArray = getGUIArray();
		// Create the GUI rect
		GUILayout.Space (5f);
		Rect r = GUILayoutUtility.GetRect (2048.0f, 200.0f, GUILayout.ExpandWidth (true));

				if (r.width != 1) {
					if (guiArray.Length != r.width) {
						guiArray = interpArray (guiArray,r.width);
				rectChange = true;
					}
				}

			if (tempWin != windowSize) {
			windowSize = Mathf.Pow (2, Mathf.Ceil (Mathf.Log (tempWin) / Mathf.Log (2.0f)));
			plugin.SetFloatParameter ("FFT Window Size", windowSize);
				 winChange = true;
			}		
			
				// If the GUI has been changed, interp GUI Array to windowSize and output to CPP plugin
		if (DrawControl (plugin, r,guiArray,Convert.ToBoolean(Mathf.RoundToInt(dispSpecFloat))) || winChange || rectChange) {
			float[] delArray = interpArray (guiArray, windowSize);
			setArray (guiArray,guiArray.Length,delArray,delArray.Length);
		}
		return true;
	}
		
	[DllImport("AudioPluginSpecDelay")]
	private static extern void getArray (out int length, out IntPtr array);

	[DllImport("AudioPluginSpecDelay")]
	private static extern void setArray (float[] gArrayIn,int gLenIn,float[] dArrayIn,int dLenIn);

	private float[] getGUIArray() {
		int arraySize;
		IntPtr arrayPtr;
		getArray (out arraySize,out arrayPtr);
		float[] theArray =  new float[arraySize];
		Marshal.Copy(arrayPtr, theArray, 0, arraySize);
		Marshal.FreeCoTaskMem (arrayPtr);
		return theArray;
	}
		
	private float[] interpArray(float[] inArray,float newSize) {
		float[] interpPoints = new float[(int)newSize];
		float[] oldX = new float[inArray.Length];
		float stepSize = (float)inArray.Length/newSize;
		for (int i = 0; i < inArray.Length; i++) {
			oldX [i] = i;
		}	
		// Get new x targets for interpolation
		for (int i = 0; i < newSize; i++) {
			interpPoints [i] = i * stepSize;
		}
		float[] outArray = CubicSpline.Compute (oldX, inArray, interpPoints);
		return outArray;
	}
		
	private bool DrawControl (IAudioEffectPlugin plugin, Rect r,float[] guiArray,bool dispSpec)
	{	
		bool changed = false;
		Event evt = Event.current;
		int controlID = GUIUtility.GetControlID (FocusType.Passive);
		EventType evtType = evt.GetTypeForControl (controlID);
		r = AudioCurveRendering.BeginCurveFrame (r);
		float guiYRange = r.yMin - r.yMax;
		float x = evt.mousePosition.x;
		float y = evt.mousePosition.y;
		if (evtType == EventType.MouseDown && r.Contains (evt.mousePosition) && evt.button == 0) {
			GUIUtility.hotControl = controlID;
			EditorGUIUtility.SetWantsMouseJumping (1);
			evt.Use ();
		} else if (evtType == EventType.MouseDrag && GUIUtility.hotControl == controlID) {
			if (x > 0 && x < r.width - 1) {
				for (int i = (-1 * brushSize); i <= brushSize; i++) {
					if ((x - i) <= r.width - 1 && (x - i) >= 0) { 
						guiArray [(int)(x - i)] = 1 - (y / Mathf.Abs (guiYRange));
					}
				}
				changed = true;
				evt.Use ();
			}
		} else if (evtType == EventType.MouseUp && evt.button == 0 && GUIUtility.hotControl == controlID) {
			GUIUtility.hotControl = 0;
			EditorGUIUtility.SetWantsMouseJumping (0);
			evt.Use ();
		}
		if (Event.current.type == EventType.Repaint) {
			if (dispSpec) {
				int specLen = (int)r.width;
				float[] spec;
				plugin.GetFloatBuffer ("InputSpec", out spec, specLen);
				DrawSpectrum (r, spec);
			}
			DrawBinMarkers (plugin, r,guiArray, guiYRange);
			GUIHelpers.DrawFrequencyTickMarks (r, sampleRate, false, tickCol);
		}
		AudioCurveRendering.EndCurveFrame ();
		return changed;
	}

	private void DrawBinMarkers (IAudioEffectPlugin plugin, Rect r,float[] guiArray, float yRange)
	{
		for (int i = 0; i < guiArray.Length; i++) {
			EditorGUI.DrawRect (new Rect (r.x + i, r.yMax, 1, yRange * guiArray [i]), binCol);
		}
	}

	private void DrawSpectrum (Rect r, float[] data)
	{
		float xscale = (float)(data.Length - 2) * 2.0f / sampleRate;
		float yscale = 1.0f / 40.0f;
		AudioCurveRendering.DrawCurve (r, delegate(float x) {
			double f = GUIHelpers.MapNormalizedFrequency ((double)x, sampleRate, false, true) * xscale;
			int i = (int)Math.Floor (f);
			double h = data [i] + (data [i + 1] - data [i]) * (f - i);
			double mag = (h > 0.0) ? (20.0f * Math.Log10 (h)) : -120.0;
			return (float)(yscale * mag);
		}, specCol);
	}
}
