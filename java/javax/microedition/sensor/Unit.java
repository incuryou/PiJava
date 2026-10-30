/*
 * Copyright 2017 Nikita Shakarun
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package javax.microedition.sensor;

import java.util.Hashtable;

// Modified for Fit11: factory-only construction and canonical validated symbols.
public class Unit {
	private static final Hashtable units = new Hashtable();
	private final String symbol;

	private Unit(String symbol) {
		this.symbol = symbol;
	}

	public static Unit getUnit(String symbol) {
		if (symbol == null) throw new NullPointerException();
		if (symbol.length() == 0) throw new IllegalArgumentException();
		synchronized (units) {
			Unit unit = (Unit)units.get(symbol);
			if (unit == null) {
				unit = new Unit(symbol);
				units.put(symbol, unit);
			}
			return unit;
		}
	}

	public String toString() {
		return symbol;
	}
}
