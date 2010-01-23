;nyquist plug-in
;version 3
;type process
;categories "http://lv2plug.in/ns/lv2core#SpectralPlugin"
;name "Vocoder..."
;action "Processing Vocoder..."
;info "by Edgar-RFT and David R. Sky\nReleased under terms of the GNU General Public License version 2\nNote: Vocoder works only on * stereo * tracks. Setting channel processing\nto '1 (right channel)' processes only the right channel of your stereo track."

;control dst "Distance: [1 to 120, default = 20]" real "" 20 1 120
;control mst "Channel processing" choice " 2 (both channels), 1 (right channel)" 0
;control bands "Number of vocoder bands" int "" 40 10 240
;control track-vl "Amplitude of original audio [percent]" real "" 100 0 100
;control noise-vl "Amplitude of white noise [percent]" real "" 0 0 100
;control radar-vl "Amplitude of Radar Needles [percent]" real "" 0 0 100
;control radar-f "Frequency of Radar Needles [Hz]" real "" 30 1 100

; vocoder by Edgar-RFT
; a bit of code added by David R. Sky
; Released under terms of the GNU Public License version 2
; http://www.opensource.org/licenses/gpl-license.php

; maybe the code once again has to be changed into _one_ local let-binding
; if you have lots of nyquist "[gc:" messages try this:
; (expand 100) ; gives Xlisp more memory but I have noticed no speed difference

; number of octaves between 20hz and 20khz
(setf octaves (/ (log 1000.0) (log 2.0)))

; convert octaves to number of steps (semitones)
(setf steps (* octaves 12.0))

; interval - number of steps per vocoder band
(setf interval (/ steps bands))

; Some useful calculations but not used in this plugin

; half tone distance in linear
; (print (exp (/ (log 2.0) 12)))

; octave distance in linear
; (print (exp (/ (log 1000.0) 40)))

; The Radar Wavetable

; make *radar-table* a global variable.
(setf contol-dummy *control-srate*)   ; save old *control-srate*
(set-control-srate *sound-srate*)  
(setf *radar-table* (pwl (/ 1.0 *control-srate*) 1.0  ; 1.0 after 1 sample
                         (/ 2.0 *control-srate*) 0.0  ; 0.0 after 2 samples
                         (/ 1.0 radar-f))) ; stay 0.0 until end of the period
(set-control-srate contol-dummy)      ; restore *control-srate*
; make *radar-table* become a nyquist wavetable of frequency radar-f
(setf *radar-table* (list *radar-table* (hz-to-step radar-f) T))

; increase the volume of the audacity track in the middle of the slider
; the sqrt trick is something like an artifical db scaling
(setf track-vol (sqrt (/ track-vl 100.0)))
; decrease the volume of the white noise in the middle of the slider
; the expt trick is an inverse db scaling
(setf noise-vol (expt (/ noise-vl 100.0) 2.0))
; also increase the volume of the needles in the middle of the slider
(setf radar-vol (sqrt (/ radar-vl 100.0)))

; here you can switch the tracks on and off for bug tracking

;  (setf radar-vol 0)
;  (setf noise-vol 0)
;  (setf track-vol 0)

; The Mixer

; calculate duration of audacity selection
(setf duration (/ len *sound-srate*))

; if track volume slider is less than 100 percent decrease track volume
(if (< track-vl 100) (setf s (vector (aref s 0) (scale track-vol (aref s 1)))))

; if radar volume slider is more than 0 percent add some radar needles
(if (> radar-vl 0) (setf s (vector (aref s 0) (sim (aref s 1)
  (scale radar-vol (osc (hz-to-step radar-f) duration *radar-table*))))))

; if noise volume slider is more than 0 percent add some white noise
(if (> noise-vl 0) (setf s (vector (aref s 0) (sim (aref s 1)
  (scale noise-vol (noise duration))))))

; The Vocoder

(defun vocoder ()
  (let ((p (+ (hz-to-step 20) (/ interval 2.0))) ; midi step of 20 Hz + offset
         f   ; we can leave f initialized to NIL
        (q (/ (sqrt 2.0) (/ octaves bands))) ; explanation still missing
        (result 0)) ; must be initialized to 0 because you cannot sum to NIL
    (dotimes (i bands)
      (setf f (step-to-hz p))
      (setf result (sum result
                   (bandpass2 (mult (lowpass8
            (s-max (bandpass2 (aref s 0) f q)
         (scale -1 (bandpass2 (aref s 0) f q))) (/ f dst))
                   (bandpass2 (aref s 1) f q)) f q)))
      (setf p (+ p interval)))
    result))

; The Program

(if (arrayp s) (let ()
  (cond ((= mst 1) (vector (aref s 0) (setf (aref s 1) (vocoder))))
        ((= mst 0) (setf s (vocoder))))
  (cond ((= mst 1) (setq peakamp (peak (aref s 1) ny:all)))
        ((= mst 0) (setq peakamp (peak s ny:all))))
  (cond ((= mst 1) (vector (aref s 0) (setf (aref s 1)
                                        (scale (/ 1.0 peakamp) (aref s 1)))))
        ((= mst 0) (setf s (scale (/ 1.0 peakamp) s))))
s))

