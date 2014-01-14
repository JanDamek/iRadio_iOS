//
//  comDamek.h
//  iRadioHD
//
//  Created by Jan Damek on 14.01.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface comDamek : NSObject

+(int)getInt:(NSDictionary*) sender;
+(NSNumber*)getNumber:(NSDictionary*) sender;
+(NSString*)getString:(NSDictionary*) sender;
+(NSString*)stringWithRemove:(NSString*) sender;

@end
