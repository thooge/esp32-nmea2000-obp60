// Function lib for display of boat data in various chart formats
#include "OBPcharts.h"

// --- Class Chart ---------------
void Chart::drawChrtHdr() {
	// chart header label + lines
	int i;
	getdisplay().fillRect(0, top, cWidth, 2, commonData->fgcolor);

	// horizontal chart labels
	getdisplay().drawLine(cStart.x, cStart.y, cWidth, cStart.y);
	getdisplay().fillRect(cStart.x, cStart.y, cWidth, 2, commonData->fgcolor);

	getdisplay().setFont(&Ubuntu_Bold10pt8b);
	getdisplay().setCursor(cStart.x, cStart.y + 12);
	getdisplay().print(dbName); // Wind data name

	getdisplay().setFont(&Ubuntu_Bold12pt8b);
	if (chrtSze == 0) {
		i = -1 * (chrtIntv / 8 - 2);
	} else {
		i = -1 * (chrtIntv / 4 - 2);
	}
	for (int j = 50; j <= (cWidth - 50); j += 50 ) {
		getdisplay().setCursor(cStart.x + j - 16, cStart.y + 12);
		getdisplay().print(i++); // time interval
		// i++;
		getdisplay().drawLine(cStart.x + j - 30, cStart.y, cStart.x - 30, cHeight + top);
	}
}

void Chart::drawChrtGrd(const int chrtRng) {
	// chart Y axis labels + lines
	int i;

	getdisplay().setFont(&Ubuntu_Bold8pt8b);
	if (chrtDir == 0) {
		i = -1 * (chrtRng / 4 - 2);
		for (int j = cStart.x; j <= (cHeight - (cHeight / 4)); j += cHeight / 4 ) {
			getdisplay().drawLine(0, cStart.y, cWidth, cStart.y);
			getdisplay().setCursor(0, cStart.y + 12);
			if (i < 10)
	            getdisplay().printf("!!%1d", i); // Range value
			else if (i < 100)
	            getdisplay().printf("!%2d", i); // Range value
			else
	            getdisplay().printf("%3d", i); // Range value
			i += (chrtRng / 4);
		}
	} else {
		i = -1 * (chrtRng / 8 - 2);
		for (int j = cStart.x; j <= (cHeight - (cHeight / 8)); j += cHeight / 8 ) {
			getdisplay().drawLine(cStart.x + j - 30, cStart.y, cStart.x - 30, cHeight + top);
			getdisplay().setCursor(0, cStart.y + 12);
			if (i < 10)
	            getdisplay().printf("!!%1d", i); // Range value
			else if (i < 100)
	            getdisplay().printf("!%2d", i); // Range value
			else
	            getdisplay().printf("%3d", i); // Range value
			i += (chrtRng / 4);
		}
	}
}

bool Chart::drawChrt(int8_t chrtIntv, int dfltRng) {
// hstryBuf = buffer to display
// bValue = present value to display additionally to chart
// chrtDir = chart direction: [0] = vertical, [1] = horizontal
// chrtSze = chart size: [0] = full size, [1] = half size left half/top, [2] half size right half/bottom
// chrtIntv = chart time interval: [1] 4 min., [2] 8 min., [3] 12 min., [4] 16 min., [5] 32 min.
// dfltRng = default range of chart, e.g. 30 = [0..30]

}
// --- Class Chart ---------------
