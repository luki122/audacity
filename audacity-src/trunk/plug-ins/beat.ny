;nyquist plug-in
;version 1
;type analyze
;categories "http://audacityteam.org/namespace#OnsetDetector"
;name "Beat Finder..."
;action "Finding beats..."
;info "Released under terms of the GNU General Public License version 2"

;control thresval "Threshold Percentage" int "" 65 5 100
(setf s1 (if (arrayp s) (snd-add (aref s 0) (aref s 1)) s))
(defun signal () (force-srate 1000 (lp (snd-follow (lp s1 50) 0.001 0.01 0.1 512) 10)))
(setq max (peak (signal) NY:ALL))
(setq thres (* (/ thresval 100.0) max))
(setq s2 (signal))
(do ((c 0.0) (l NIL) (p T) (v (snd-fetch s2))) ((not v) l)
 (if (and p (> v thres)) (setq l (cons (list c "B") l)))
 (setq p (< v thres))
 (setq c (+ c 0.001))
 (setq v (snd-fetch s2)))

; arch-tag: 2204686b-2dcc-4891-964a-2749ac30661b

